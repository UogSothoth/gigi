#include "../public/technique.h"
#include "DX12Utils/dxutils.h"
#include "DX12Utils/DelayedReleaseTracker.h"
#include "DX12Utils/HeapAllocationTracker.h"
#include "DX12Utils/TextureCache.h"

#include <vector>
#include <chrono>

namespace simpleRaster
{
    static std::vector<Context*> s_allContexts;

    static DX12Utils::Heap                  s_srvHeap;
    static DX12Utils::Heap                  s_rtvHeap;
    static DX12Utils::Heap                  s_dsvHeap;
    static DX12Utils::UploadBufferTracker   s_ubTracker;
    static DX12Utils::DelayedReleaseTracker s_delayedRelease;
    static DX12Utils::HeapAllocationTracker s_heapAllocationTrackerRTV;
    static DX12Utils::HeapAllocationTracker s_heapAllocationTrackerDSV;

    TLogFn Context::LogFn = [] (LogLevel level, const char* msg, ...) {};
    TPerfEventBeginFn Context::PerfEventBeginFn = [] (const char* name, ID3D12GraphicsCommandList* commandList, int index) {};
    TPerfEventEndFn Context::PerfEventEndFn = [] (ID3D12GraphicsCommandList* commandList) {};

    std::wstring Context::s_techniqueLocation = L"./";
    static unsigned int s_timerIndex = 0;

    ID3D12CommandSignature* ContextInternal::s_commandSignatureDispatch = nullptr;

    template <typename T>
    T Pow2GE(const T& A)
    {
        float f = std::log2(float(A));
        f = std::ceilf(f);
        return (T)std::pow(2.0f, f);
    }

    bool CreateShared(ID3D12Device* device)
    {

        // Create heaps
        if (c_numSRVDescriptors > 0 && !CreateHeap(s_srvHeap, device, c_numSRVDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, Context::LogFn))
            return false;

        if (c_numRTVDescriptors > 0 && !CreateHeap(s_rtvHeap, device, c_numRTVDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, Context::LogFn))
            return false;

        if (c_numDSVDescriptors > 0 && !CreateHeap(s_dsvHeap, device, c_numDSVDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, Context::LogFn))
            return false;

        s_heapAllocationTrackerRTV.Init(s_rtvHeap.m_heap, c_numRTVDescriptors, (int)device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
        s_heapAllocationTrackerDSV.Init(s_dsvHeap.m_heap, c_numDSVDescriptors, (int)device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

        // create indirect dispatch command
        {
            D3D12_INDIRECT_ARGUMENT_DESC dispatchArg = {};
            dispatchArg.Type						 = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

            D3D12_COMMAND_SIGNATURE_DESC dispatchDesc = {};
            dispatchDesc.ByteStride					  = sizeof(uint32_t) * 3;
            dispatchDesc.NumArgumentDescs			  = 1;
            dispatchDesc.pArgumentDescs				  = &dispatchArg;
            dispatchDesc.NodeMask					  = 0x0;

            device->CreateCommandSignature(
                &dispatchDesc,
                nullptr,
                IID_PPV_ARGS(&ContextInternal::s_commandSignatureDispatch));
        }

        return true;
    }

    void DestroyShared()
    {

        // Clear out heap trackers
        s_heapAllocationTrackerRTV.Release();
        s_heapAllocationTrackerDSV.Release();

        // Destroy Heaps
        DestroyHeap(s_srvHeap);
        DestroyHeap(s_rtvHeap);
        DestroyHeap(s_dsvHeap);

        // Destroy any upload buffers
        s_ubTracker.Release();

        // Finish any delayed release
        s_delayedRelease.Release();

        // Destroy indirect dispatch command
        if (ContextInternal::s_commandSignatureDispatch)
        {
            ContextInternal::s_commandSignatureDispatch->Release();
            ContextInternal::s_commandSignatureDispatch = nullptr;
        }
    }

    Context* CreateContext(ID3D12Device* device)
    {
        if (s_allContexts.size() == 0)
        {
            if (!CreateShared(device))
                return nullptr;
        }

        Context* ret = new Context;
        s_allContexts.push_back(ret);
        return ret;
    }

    void DestroyContext(Context* context)
    {
        s_allContexts.erase(std::remove(s_allContexts.begin(), s_allContexts.end(), context), s_allContexts.end());
        delete context;
        if (s_allContexts.size() == 0)
            DestroyShared();
    }

    void OnNewFrame(int framesInFlight)
    {
        s_delayedRelease.OnNewFrame(framesInFlight);
        s_ubTracker.OnNewFrame(framesInFlight);
        s_heapAllocationTrackerRTV.OnNewFrame(framesInFlight);
        s_heapAllocationTrackerDSV.OnNewFrame(framesInFlight);
    }

    int Context::GetContextCount()
    {
        return (int)s_allContexts.size();
    }

    Context* Context::GetContext(int index)
    {
        if (index >= 0 && index < GetContextCount())
            return s_allContexts[index];
        else
            return nullptr;
    }

    ID3D12Resource* Context::CreateManagedBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_FLAGS flags, const void* data, size_t size, const wchar_t* debugName, D3D12_RESOURCE_STATES desiredState)
    {
        // Make a buffer and have the context manage it
        ID3D12Resource* ret = DX12Utils::CreateBuffer(
            device,
            (unsigned int)size,
            flags,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_HEAP_TYPE_DEFAULT,
            debugName,
            Context::LogFn
        );
        AddManagedResource(ret);

        // Copy the data to the buffer if we should
        if (data != nullptr && size > 0)
            UploadBufferData(device, commandList, ret, D3D12_RESOURCE_STATE_COPY_DEST, data, (unsigned int)size);

        // Do a resource transition if we should
        if (desiredState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier;

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = ret;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = desiredState;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &barrier);
        }

        // return the resource
        return ret;
    }

    ID3D12Resource* Context::CreateManagedTexture(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_FLAGS flags, DXGI_FORMAT format, const unsigned int size[3], DX12Utils::ResourceType resourceType, const void* initialData, const wchar_t* debugName, D3D12_RESOURCE_STATES desiredState)
    {
        // Create a texture
        ID3D12Resource* ret = DX12Utils::CreateTexture(device, size, format, flags, D3D12_RESOURCE_STATE_COPY_DEST, resourceType, debugName, Context::LogFn);
        AddManagedResource(ret);

        // copy initial data in, if we should
        if (initialData != nullptr)
        {
            DX12Utils::DXGI_FORMAT_Info formatInfo = DX12Utils::Get_DXGI_FORMAT_Info(format, Context::LogFn);
            UploadTextureData(device, commandList, ret, D3D12_RESOURCE_STATE_COPY_DEST, initialData, size[0] * formatInfo.bytesPerPixel);
        }

        // Put the resource into the desired state
        if (desiredState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier;

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = ret;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = desiredState;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &barrier);
        }

        return ret;
    }

    ID3D12Resource* Context::CreateManagedTextureAndClear(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_FLAGS flags, DXGI_FORMAT format, const unsigned int size[3], DX12Utils::ResourceType resourceType, void* clearValue, size_t clearValueSize, const wchar_t* debugName, D3D12_RESOURCE_STATES desiredState)
    {
        // Make sure the clear value is the correct size
        DX12Utils::DXGI_FORMAT_Info formatInfo = DX12Utils::Get_DXGI_FORMAT_Info(format, Context::LogFn);
        if (clearValue != nullptr && clearValueSize > 0 && clearValueSize != formatInfo.bytesPerPixel)
            return nullptr;

        // Copy data into the resource
        std::vector<unsigned char> expandedClearValue;
        void* initialData = nullptr;
        if (clearValue != nullptr && clearValueSize > 0)
        {
            expandedClearValue.resize(size[0] * size[1] * size[2] * formatInfo.bytesPerPixel);
            unsigned char* dest = expandedClearValue.data();
            for (size_t i = 0; i < size[0] * size[1] * size[2]; ++i)
            {
                memcpy(dest, clearValue, formatInfo.bytesPerPixel);
                dest += formatInfo.bytesPerPixel;
            }
            initialData = expandedClearValue.data();
        }

        // make and return the texture
        return CreateManagedTexture(device, commandList, flags, format, size, resourceType, initialData, debugName, desiredState);
    }

    ID3D12Resource* Context::CreateManagedTextureFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_FLAGS flags, DXGI_FORMAT format, DX12Utils::ResourceType resourceType, const char* fileName, bool sourceIsSRGB, unsigned int size[3], const wchar_t* debugName, D3D12_RESOURCE_STATES desiredState)
    {
        // Get the desired channel type
        DX12Utils::DXGI_FORMAT_Info formatInfo = DX12Utils::Get_DXGI_FORMAT_Info(format, Context::LogFn);
        DX12Utils::TextureCache::Type desiredChannelType = DX12Utils::TextureCache::Type::U8;
        if (formatInfo.channelType == DX12Utils::DXGI_FORMAT_Info::ChannelType::_uint8_t)
            desiredChannelType = DX12Utils::TextureCache::Type::U8;
        else if (formatInfo.channelType == DX12Utils::DXGI_FORMAT_Info::ChannelType::_float)
            desiredChannelType = DX12Utils::TextureCache::Type::F32;
        else
            return nullptr;

        if (resourceType == DX12Utils::ResourceType::Texture2D)
        {
            // Load the texture and convert as necessary
            DX12Utils::TextureCache::Texture texture = DX12Utils::TextureCache::GetAs(fileName, sourceIsSRGB, desiredChannelType, formatInfo.sRGB, formatInfo.channelCount);
            if (!texture.Valid())
                return nullptr;

            // store off image properties
            size[0] = texture.width;
            size[1] = texture.height;
            size[2] = 1;

            // make and return the texture
            return CreateManagedTexture(device, commandList, flags, format, size, resourceType, texture.pixels.data(), debugName, desiredState);
        }
        else if (resourceType == DX12Utils::ResourceType::Texture2DArray ||
                 resourceType == DX12Utils::ResourceType::Texture3D ||
                 resourceType == DX12Utils::ResourceType::TextureCube)
        {
            static const char* c_cubeMapNames[] =
            {
                "Right",
                "Left",
                "Up",
                "Down",
                "Front",
                "Back"
            };

            bool useCubeMapNames = (resourceType == DX12Utils::ResourceType::TextureCube && strstr(fileName, "%s") != nullptr);
            bool hasPercentI = strstr(fileName, "%i") != nullptr;
            if (!useCubeMapNames && !hasPercentI)
                return nullptr;

            std::vector<DX12Utils::TextureCache::Texture> loadedTextureSlices;

            // Load multiple textures
            int textureIndex = -1;
            while (1)
            {
                textureIndex++;
                char indexedFileName[1024];

                if (useCubeMapNames)
                    sprintf_s(indexedFileName, fileName, c_cubeMapNames[textureIndex]);
                else
                    sprintf_s(indexedFileName, fileName, textureIndex);

                // Load the texture and convert as necessary
                DX12Utils::TextureCache::Texture loadedTextureSlice = DX12Utils::TextureCache::GetAs(indexedFileName, sourceIsSRGB, desiredChannelType, formatInfo.sRGB, formatInfo.channelCount);
                if (!loadedTextureSlice.Valid())
                {
                    if (textureIndex == 0)
                        return nullptr;
                    break;
                }

                // make sure the textures are the same size
                if (textureIndex > 0 && (loadedTextureSlice.width != loadedTextureSlices[0].width || loadedTextureSlice.height != loadedTextureSlices[0].height))
                    return nullptr;

                loadedTextureSlices.push_back(loadedTextureSlice);
            }

            // store the texture size
            size[0] = loadedTextureSlices[0].width;
            size[1] = loadedTextureSlices[0].height;
            size[2] = (unsigned int)loadedTextureSlices.size();

            // gather up all pixels into a contiguous chunk of memory
            std::vector<unsigned char> allPixels;
            for (const DX12Utils::TextureCache::Texture& texture : loadedTextureSlices)
                allPixels.insert(allPixels.end(), texture.pixels.begin(), texture.pixels.end());

            // make and return the texture
            return CreateManagedTexture(device, commandList, flags, format, size, resourceType, allPixels.data(), debugName, desiredState);
        }
        else
            return nullptr;
    }

    void Context::UploadTextureData(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ID3D12Resource* texture, D3D12_RESOURCE_STATES textureState, const void* data, unsigned int unalignedPitch)
    {
        // Get information about the texture
        int alignedPitch = ALIGN(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, unalignedPitch);
        D3D12_RESOURCE_DESC textureDesc = texture->GetDesc();

        // transition the resource to copy dest if it isn't already
        if (textureState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier;

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = texture;
            barrier.Transition.StateBefore = textureState;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &barrier);
        }

        // 3d textures do a single copy because it's a single sub resource.
        if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
        {
            // Get the upload buffer
            DX12Utils::UploadBufferTracker::UploadBufferTracker::Buffer* uploadBuffer = s_ubTracker.GetBuffer(device, alignedPitch * textureDesc.Height * textureDesc.DepthOrArraySize, Context::LogFn, false);

            // Map, copy, unmap
            {
                unsigned char* dest = nullptr;
                D3D12_RANGE readRange = { 0, 0 };
                HRESULT hr = uploadBuffer->buffer->Map(0, &readRange, (void**)&dest);
                if (FAILED(hr))
                {
                    Context::LogFn(LogLevel::Error, "Could not map upload buffer.");
                }
                else
                {
                    const unsigned char* src = (const unsigned char*)data;
                    for (int iz = 0; iz < textureDesc.DepthOrArraySize; ++iz)
                    {
                        for (int iy = 0; iy < (int)textureDesc.Height; ++iy)
                        {
                            memcpy(dest, src, unalignedPitch);
                            src += unalignedPitch;
                            dest += alignedPitch;
                        }
                    }

                    uploadBuffer->buffer->Unmap(0, nullptr);
                }
            }

            // copy the upload buffer into the texture
            {
                unsigned char layoutMem[sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)];
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layout = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)layoutMem;
                device->GetCopyableFootprints(&textureDesc, 0, 1, 0, layout, nullptr, nullptr, nullptr);

                D3D12_TEXTURE_COPY_LOCATION src = {};
                src.pResource = uploadBuffer->buffer;
                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src.PlacedFootprint = *layout;

                D3D12_TEXTURE_COPY_LOCATION dest = {};
                dest.pResource = texture;
                dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dest.SubresourceIndex = 0;

                commandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
            }
        }
        // 2d array textures do a copy for each slice
        else if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        {
            for (int iz = 0; iz < textureDesc.DepthOrArraySize; ++iz)
            {
                // Get the upload buffer
                DX12Utils::UploadBufferTracker::Buffer* uploadBuffer = s_ubTracker.GetBuffer(device, alignedPitch * textureDesc.Height, Context::LogFn, false);

                // Map, copy, unmap
                {
                    unsigned char* dest = nullptr;
                    D3D12_RANGE readRange = { 0, 0 };
                    HRESULT hr = uploadBuffer->buffer->Map(0, &readRange, (void**)&dest);
                    if (FAILED(hr))
                    {
                        Context::LogFn(LogLevel::Error, "Could not map upload buffer.");
                    }
                    else
                    {
                        const unsigned char* src = &((const unsigned char*)data)[unalignedPitch * textureDesc.Height * iz];
                        for (int iy = 0; iy < (int)textureDesc.Height; ++iy)
                        {
                            memcpy(dest, src, unalignedPitch);
                            src += unalignedPitch;
                            dest += alignedPitch;
                        }

                        uploadBuffer->buffer->Unmap(0, nullptr);
                    }
                }

                 // copy the upload buffer into the texture
                 {
                     unsigned char layoutMem[sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)];
                     D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layout = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)layoutMem;
                     device->GetCopyableFootprints(&textureDesc, 0, 1, 0, layout, nullptr, nullptr, nullptr);

                     D3D12_TEXTURE_COPY_LOCATION src = {};
                     src.pResource = uploadBuffer->buffer;
                     src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                     src.PlacedFootprint = *layout;

                     D3D12_TEXTURE_COPY_LOCATION dest = {};
                     dest.pResource = texture;
                     dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                     dest.SubresourceIndex = iz;

                     commandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
                 }
            }
        }
        else
        {
            Context::LogFn(LogLevel::Error, "Unhandled texture dimension.");
        }

        // transition the resource back to what it was
        if (textureState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier;

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = texture;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = textureState;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &barrier);
        }
    }

    void Context::UploadBufferData(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ID3D12Resource* buffer, D3D12_RESOURCE_STATES bufferState, const void* data, unsigned int dataSize)
    {
        // Get the upload buffer
        DX12Utils::UploadBufferTracker::UploadBufferTracker::Buffer* uploadBuffer = s_ubTracker.GetBuffer(device, dataSize, Context::LogFn, false);

        // copy cpu data to the upload buffer
        {
            void* start = nullptr;
            HRESULT hr = uploadBuffer->buffer->Map(0, nullptr, reinterpret_cast<void**>(&start));
            if(hr)
            {
                Context::LogFn(LogLevel::Error, "Could not map upload buffer");
                return;
            }

            memcpy(start, data, dataSize);

            uploadBuffer->buffer->Unmap(0, nullptr);
        }

        // transition the resource to copy dest if it isn't already
        if (bufferState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier;

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = buffer;
            barrier.Transition.StateBefore = bufferState;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &barrier);
        }

        // copy the resource
        commandList->CopyResource(buffer, uploadBuffer->buffer);

        // transition the resource back to what it was
        if (bufferState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier;

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = buffer;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = bufferState;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &barrier);
        }
    }

    bool Context::CreateManagedRTV(ID3D12Device* device, ID3D12Resource* resource, DXGI_FORMAT format, D3D12_RTV_DIMENSION dimension, int sliceIndex, int& rtvIndex, const char* debugText)
    {
        if (dimension != D3D12_RTV_DIMENSION_TEXTURE2D && dimension != D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
        {
            Context::LogFn(LogLevel::Error, "unhandled RTV texture dimension type.");
            return false;
        }

        // Allocate handle
        if (!s_heapAllocationTrackerRTV.Allocate(rtvIndex, debugText))
            return false;

        // Create RTV
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format = format;
        rtvDesc.ViewDimension = dimension;
        if (dimension == D3D12_RTV_DIMENSION_TEXTURE2D)
        {
            rtvDesc.Texture2D.MipSlice = 0;
            rtvDesc.Texture2D.PlaneSlice = 0;
        }
        else
        {
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.PlaneSlice = 0;
            rtvDesc.Texture2DArray.ArraySize = 1;
            rtvDesc.Texture2DArray.FirstArraySlice = sliceIndex;
        }

        device->CreateRenderTargetView(resource, &rtvDesc, s_heapAllocationTrackerRTV.GetCPUHandle(rtvIndex));

        m_internal.m_managedRTVs.push_back(rtvIndex);

        return true;
    }

    bool Context::CreateManagedDSV(ID3D12Device* device, ID3D12Resource* resource, DXGI_FORMAT format, D3D12_DSV_DIMENSION dimension, int sliceIndex, int& dsvIndex, const char* debugText)
    {
        if (dimension != D3D12_DSV_DIMENSION_TEXTURE2D && dimension != D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
        {
            Context::LogFn(LogLevel::Error, "unhandled RTV texture dimension type.");
            return false;
        }

        // Allocate handle
        if (!s_heapAllocationTrackerDSV.Allocate(dsvIndex, debugText))
            return false;

        // Create DSV
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Format = DX12Utils::DSV_Safe_DXGI_FORMAT(format);
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension = dimension;

        if (dimension == D3D12_RTV_DIMENSION_TEXTURE2D)
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }
        else
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = sliceIndex;
            dsvDesc.Texture2DArray.ArraySize = 1;
        }

        device->CreateDepthStencilView(resource, &dsvDesc, s_heapAllocationTrackerDSV.GetCPUHandle(dsvIndex));

        m_internal.m_managedDSVs.push_back(dsvIndex);

        return true;
    }

    const ProfileEntry* Context::ReadbackProfileData(ID3D12CommandQueue* commandQueue, int& numItems)
    {
        numItems = 0;

        if (!m_profile || !m_internal.m_TimestampReadbackBuffer)
            return nullptr;

        uint64_t GPUFrequency;
        commandQueue->GetTimestampFrequency(&GPUFrequency);
        double GPUTickDelta = 1.0 / static_cast<double>(GPUFrequency);

        D3D12_RANGE range;
        range.Begin = 0;
        range.End = ((1 + 1) * 2) * sizeof(uint64_t);

        uint64_t* timeStampBuffer = nullptr;
        m_internal.m_TimestampReadbackBuffer->Map(0, &range, (void**)&timeStampBuffer);

        m_profileData[numItems].m_gpu = float(GPUTickDelta * double(timeStampBuffer[numItems*2+2] - timeStampBuffer[numItems*2+1])); numItems++; // Draw Call: Rasterize
        m_profileData[numItems].m_gpu = float(GPUTickDelta * double(timeStampBuffer[numItems*2+1] - timeStampBuffer[0])); numItems++; // GPU total

        D3D12_RANGE emptyRange = {};
        m_internal.m_TimestampReadbackBuffer->Unmap(0, &emptyRange);

        return m_profileData;
    }

    Context::~Context()
    {
        for (int index : m_internal.m_managedRTVs)
            s_heapAllocationTrackerRTV.Free(index);
        m_internal.m_managedRTVs.clear();

        for (int index : m_internal.m_managedDSVs)
            s_heapAllocationTrackerDSV.Free(index);
        m_internal.m_managedDSVs.clear();

        for (ID3D12Resource* resource : m_internal.m_managedResources)
            resource->Release();
        m_internal.m_managedResources.clear();

        if(m_internal.m_TimestampQueryHeap)
        {
            m_internal.m_TimestampQueryHeap->Release();
            m_internal.m_TimestampQueryHeap = nullptr;
        }

        if(m_internal.m_TimestampReadbackBuffer)
        {
            m_internal.m_TimestampReadbackBuffer->Release();
            m_internal.m_TimestampReadbackBuffer = nullptr;
        }

        if(m_output.texture_Color_Buffer)
        {
            s_delayedRelease.Add(m_output.texture_Color_Buffer);
            m_output.texture_Color_Buffer = nullptr;
        }

        if(m_output.texture_Color_Buffer_rtv != -1)
        {
            s_heapAllocationTrackerRTV.Free(m_output.texture_Color_Buffer_rtv);
            m_output.texture_Color_Buffer_rtv = -1;
        }

        if(m_output.texture_Depth_Buffer)
        {
            s_delayedRelease.Add(m_output.texture_Depth_Buffer);
            m_output.texture_Depth_Buffer = nullptr;
        }

        if(m_output.texture_Depth_Buffer_dsv != -1)
        {
            s_heapAllocationTrackerRTV.Free(m_output.texture_Depth_Buffer_dsv);
            m_output.texture_Depth_Buffer_dsv = -1;
        }

        // _VertexShaderCB
        if (m_internal.constantBuffer__VertexShaderCB)
        {
            s_delayedRelease.Add(m_internal.constantBuffer__VertexShaderCB);
            m_internal.constantBuffer__VertexShaderCB = nullptr;
        }

        if(m_internal.drawCall_Rasterize_pso)
        {
            s_delayedRelease.Add(m_internal.drawCall_Rasterize_pso);
            m_internal.drawCall_Rasterize_pso = nullptr;
        }

        if(m_internal.drawCall_Rasterize_rootSig)
        {
            s_delayedRelease.Add(m_internal.drawCall_Rasterize_rootSig);
            m_internal.drawCall_Rasterize_rootSig = nullptr;
        }
    }

    void Execute(Context* context, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
    {
        // reset the timer index
        s_timerIndex = 0;

        ScopedPerfEvent scopedPerf("simpleRaster", commandList, 5);

        std::chrono::high_resolution_clock::time_point startPointCPUTechnique;
        if(context->m_profile)
        {
            startPointCPUTechnique = std::chrono::high_resolution_clock::now();
            if(context->m_internal.m_TimestampQueryHeap == nullptr)
            {
                D3D12_QUERY_HEAP_DESC QueryHeapDesc;
                QueryHeapDesc.Count = (1+1) * 2;
                QueryHeapDesc.NodeMask = 1;
                QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
                device->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(&context->m_internal.m_TimestampQueryHeap));
                if (c_debugNames)
                    context->m_internal.m_TimestampQueryHeap->SetName(L"simpleRaster Time Stamp Query Heap");

                context->m_internal.m_TimestampReadbackBuffer = DX12Utils::CreateBuffer(device, sizeof(uint64_t) * (1+1) * 2, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK, (c_debugNames ? L"simpleRaster Time Stamp Query Heap" : nullptr), nullptr);
            }
            commandList->EndQuery(context->m_internal.m_TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, s_timerIndex++);
        }

        if (!context->m_input.buffer_VertexBuffer)
        {
            Context::LogFn(LogLevel::Error, "simpleRaster: Imported buffer \"VertexBuffer\" is null.\n");
            return;
        }

        if (context->m_input.buffer_VertexBuffer_vertexInputLayout.size() == 0)
        {
            Context::LogFn(LogLevel::Error, "simpleRaster: Imported buffer \"VertexBuffer\" is used as a vertex buffer but no vertexInputLayout was given.\n");
            return;
        }

        // Make sure internally owned resources are created and are the right size and format
        context->EnsureResourcesCreated(device, commandList);

        // set the heaps
        ID3D12DescriptorHeap* heaps[] =
        {
            s_srvHeap.m_heap,
        };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        // Make sure imported resources are in the correct state
        {
            int barrierCount = 0;
            D3D12_RESOURCE_BARRIER barriers[1];

            if(context->m_input.buffer_VertexBuffer_state != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
            {
                barriers[barrierCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barriers[barrierCount].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barriers[barrierCount].Transition.pResource = context->m_input.buffer_VertexBuffer;
                barriers[barrierCount].Transition.StateBefore = context->m_input.buffer_VertexBuffer_state;
                barriers[barrierCount].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
                barriers[barrierCount].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrierCount++;
            }

            if(barrierCount > 0)
                commandList->ResourceBarrier(barrierCount, barriers);
        }

        // Shader Constants: _VertexShaderCB
        {
            context->m_internal.constantBuffer__VertexShaderCB_cpu.ViewProjMtx = context->m_input.variable_ViewProjMtx;
            DX12Utils::CopyConstantsCPUToGPU(s_ubTracker, device, commandList, context->m_internal.constantBuffer__VertexShaderCB, context->m_internal.constantBuffer__VertexShaderCB_cpu, Context::LogFn);
        }

        // Draw Call: Rasterize
        {
            ScopedPerfEvent scopedPerf("Draw Call: Rasterize", commandList, 0);
            std::chrono::high_resolution_clock::time_point startPointCPU;
            if(context->m_profile)
            {
                startPointCPU = std::chrono::high_resolution_clock::now();
                commandList->EndQuery(context->m_internal.m_TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, s_timerIndex++);
            }

            commandList->SetGraphicsRootSignature(context->m_internal.drawCall_Rasterize_rootSig);
            commandList->SetPipelineState(context->m_internal.drawCall_Rasterize_pso);

            DX12Utils::ResourceDescriptor descriptorsVS[] =
            {
                { context->m_internal.constantBuffer__VertexShaderCB, DXGI_FORMAT_UNKNOWN, DX12Utils::AccessType::CBV, DX12Utils::ResourceType::Buffer, false, 256, 1 },
            };
            D3D12_GPU_DESCRIPTOR_HANDLE descriptorTableVS = GetDescriptorTable(device, s_srvHeap, descriptorsVS, 1, Context::LogFn);
            commandList->SetGraphicsRootDescriptorTable(0, descriptorTableVS);

            // Vertex Buffer
            int vertexCountPerInstance = context->m_input.buffer_VertexBuffer_count;

            D3D12_VERTEX_BUFFER_VIEW vbView;
            vbView.BufferLocation = context->m_input.buffer_VertexBuffer->GetGPUVirtualAddress();
            vbView.StrideInBytes = (context->m_input.buffer_VertexBuffer_format == DXGI_FORMAT_UNKNOWN)
                ? context->m_input.buffer_VertexBuffer_stride
                : DX12Utils::Get_DXGI_FORMAT_Info(context->m_input.buffer_VertexBuffer_format, Context::LogFn).bytesPerPixel;
            vbView.SizeInBytes = vbView.StrideInBytes * context->m_input.buffer_VertexBuffer_count;

            commandList->IASetVertexBuffers(0, 1, &vbView);

            // Index Buffer
            int indexCountPerInstance = -1;

            // Instance Buffer
            int instanceCount = 1;

            // Clear Color_Buffer
            {
                 float clearValues[4] = { 0.200000f, 0.200000f, 0.200000f, 1.000000f };
                 commandList->ClearRenderTargetView(s_heapAllocationTrackerRTV.GetCPUHandle(context->m_output.texture_Color_Buffer_rtv), clearValues, 0, nullptr);
            }

            // Clear Depth_Buffer
            {
                D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
                commandList->ClearDepthStencilView(s_heapAllocationTrackerDSV.GetCPUHandle(context->m_output.texture_Depth_Buffer_dsv), clearFlags, 0.000000f, 0, 0, nullptr);
            }

            int renderWidth = context->m_output.texture_Color_Buffer_size[0];
            int renderHeight = context->m_output.texture_Color_Buffer_size[1];

            D3D12_CPU_DESCRIPTOR_HANDLE colorTargetHandles[] =
            {
                s_heapAllocationTrackerRTV.GetCPUHandle(context->m_output.texture_Color_Buffer_rtv),
            };

            int colorTargetHandleCount = _countof(colorTargetHandles);

            D3D12_CPU_DESCRIPTOR_HANDLE depthTargetHandle = s_heapAllocationTrackerDSV.GetCPUHandle(context->m_output.texture_Depth_Buffer_dsv);
            D3D12_CPU_DESCRIPTOR_HANDLE* depthTargetHandlePtr = &depthTargetHandle;

            // clear viewport and scissor rect
            D3D12_VIEWPORT viewport = { 0.0f, 0.0f, float(renderWidth), float(renderHeight), 0.0f, 1.0f };
            D3D12_RECT scissorRect = { 0, 0, (LONG)renderWidth, (LONG)renderHeight };
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissorRect);

            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->OMSetStencilRef(0);

            commandList->OMSetRenderTargets(colorTargetHandleCount, colorTargetHandles, false, depthTargetHandlePtr);

            commandList->DrawInstanced(vertexCountPerInstance, instanceCount, 0, 0);

            if(context->m_profile)
            {
                context->m_profileData[(s_timerIndex-1)/2].m_label = "Rasterize";
                context->m_profileData[(s_timerIndex-1)/2].m_cpu = (float)std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - startPointCPU).count();
                commandList->EndQuery(context->m_internal.m_TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, s_timerIndex++);
            }
        }

        // Make sure imported resources are put back in the state they were given to us in
        {
            int barrierCount = 0;
            D3D12_RESOURCE_BARRIER barriers[1];

            if(context->m_input.buffer_VertexBuffer_state != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
            {
                barriers[barrierCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barriers[barrierCount].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barriers[barrierCount].Transition.pResource = context->m_input.buffer_VertexBuffer;
                barriers[barrierCount].Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
                barriers[barrierCount].Transition.StateAfter = context->m_input.buffer_VertexBuffer_state;
                barriers[barrierCount].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrierCount++;
            }

            if(barrierCount > 0)
                commandList->ResourceBarrier(barrierCount, barriers);
        }

        if(context->m_profile)
        {
            context->m_profileData[(s_timerIndex-1)/2].m_label = "Total";
            context->m_profileData[(s_timerIndex-1)/2].m_cpu = (float)std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - startPointCPUTechnique).count();
            commandList->EndQuery(context->m_internal.m_TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, s_timerIndex++);
            commandList->ResolveQueryData(context->m_internal.m_TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, s_timerIndex, context->m_internal.m_TimestampReadbackBuffer, 0);
        }
    }

    void Context::EnsureResourcesCreated(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
    {
        bool dirty = false;

        // Color_Buffer
        {
            unsigned int baseSize[3] = { 1, 1, 1 };

            unsigned int desiredSize[3] = {
                ((baseSize[0] + 0) * 512) / 1 + 0,
                ((baseSize[1] + 0) * 512) / 1 + 0,
                ((baseSize[2] + 0) * 1) / 1 + 0
            };

            DXGI_FORMAT desiredFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

            if(!m_output.texture_Color_Buffer ||
               m_output.texture_Color_Buffer_size[0] != desiredSize[0] ||
               m_output.texture_Color_Buffer_size[1] != desiredSize[1] ||
               m_output.texture_Color_Buffer_size[2] != desiredSize[2] ||
               m_output.texture_Color_Buffer_format != desiredFormat)
            {
                dirty = true;
                if(m_output.texture_Color_Buffer)
                    s_delayedRelease.Add(m_output.texture_Color_Buffer);

                m_output.texture_Color_Buffer = DX12Utils::CreateTexture(device, desiredSize, desiredFormat, m_output.texture_Color_Buffer_flags, D3D12_RESOURCE_STATE_RENDER_TARGET, DX12Utils::ResourceType::Texture2D, (c_debugNames ? L"Color_Buffer" : nullptr), Context::LogFn);
                m_output.texture_Color_Buffer_size[0] = desiredSize[0];
                m_output.texture_Color_Buffer_size[1] = desiredSize[1];
                m_output.texture_Color_Buffer_size[2] = desiredSize[2];
                m_output.texture_Color_Buffer_format = desiredFormat;

                // Allocate a RTV handle
                if(m_output.texture_Color_Buffer_rtv == -1 && !s_heapAllocationTrackerRTV.Allocate(m_output.texture_Color_Buffer_rtv, "Color_Buffer"))
                    Context::LogFn(LogLevel::Error, "Ran out of RTV descriptors, please increase c_numRTVDescriptors");

                // Create RTV
                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
                rtvDesc.Format = m_output.texture_Color_Buffer_format;
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = 0;
                rtvDesc.Texture2D.PlaneSlice = 0;
                device->CreateRenderTargetView(m_output.texture_Color_Buffer, &rtvDesc, s_heapAllocationTrackerRTV.GetCPUHandle(m_output.texture_Color_Buffer_rtv));
            }
        }

        // Depth_Buffer
        {
            unsigned int baseSize[3] = {
                m_output.texture_Color_Buffer_size[0],
                m_output.texture_Color_Buffer_size[1],
                m_output.texture_Color_Buffer_size[2]
            };

            unsigned int desiredSize[3] = {
                ((baseSize[0] + 0) * 1) / 1 + 0,
                ((baseSize[1] + 0) * 1) / 1 + 0,
                ((baseSize[2] + 0) * 1) / 1 + 0
            };

            DXGI_FORMAT desiredFormat = DXGI_FORMAT_D32_FLOAT;

            if(!m_output.texture_Depth_Buffer ||
               m_output.texture_Depth_Buffer_size[0] != desiredSize[0] ||
               m_output.texture_Depth_Buffer_size[1] != desiredSize[1] ||
               m_output.texture_Depth_Buffer_size[2] != desiredSize[2] ||
               m_output.texture_Depth_Buffer_format != desiredFormat)
            {
                dirty = true;
                if(m_output.texture_Depth_Buffer)
                    s_delayedRelease.Add(m_output.texture_Depth_Buffer);

                m_output.texture_Depth_Buffer = DX12Utils::CreateTexture(device, desiredSize, desiredFormat, m_output.texture_Depth_Buffer_flags, D3D12_RESOURCE_STATE_DEPTH_WRITE, DX12Utils::ResourceType::Texture2D, (c_debugNames ? L"Depth_Buffer" : nullptr), Context::LogFn);
                m_output.texture_Depth_Buffer_size[0] = desiredSize[0];
                m_output.texture_Depth_Buffer_size[1] = desiredSize[1];
                m_output.texture_Depth_Buffer_size[2] = desiredSize[2];
                m_output.texture_Depth_Buffer_format = desiredFormat;

                // Allocate a DSV handle
                if(m_output.texture_Depth_Buffer_dsv == -1 && !s_heapAllocationTrackerDSV.Allocate(m_output.texture_Depth_Buffer_dsv, "Depth_Buffer"))
                    Context::LogFn(LogLevel::Error, "Ran out of DSV descriptors, please increase c_numDSVDescriptors");

                // Create DSV
                D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
                dsvDesc.Format = DX12Utils::DSV_Safe_DXGI_FORMAT(m_output.texture_Depth_Buffer_format);
                dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsvDesc.Texture2D.MipSlice = 0;
                device->CreateDepthStencilView(m_output.texture_Depth_Buffer, &dsvDesc, s_heapAllocationTrackerDSV.GetCPUHandle(m_output.texture_Depth_Buffer_dsv));
            }
        }

        // _VertexShaderCB
        if (m_internal.constantBuffer__VertexShaderCB == nullptr)
        {
            dirty = true;
            m_internal.constantBuffer__VertexShaderCB = DX12Utils::CreateBuffer(device, 256, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE_DEFAULT, (c_debugNames ? L"_VertexShaderCB" : nullptr), Context::LogFn);
        }
        EnsureDrawCallPSOsCreated(device, dirty);
    }

    bool Context::EnsureDrawCallPSOsCreated(ID3D12Device* device, bool dirty)
    {

        // Draw Call: Rasterize
        if (dirty) // TODO: This is too heavy handed. We should only (re)create when needed.
        {
            if (m_internal.drawCall_Rasterize_pso)
            {
                s_delayedRelease.Add(m_internal.drawCall_Rasterize_pso);
                m_internal.drawCall_Rasterize_pso = nullptr;
            }
            if (m_internal.drawCall_Rasterize_rootSig)
            {
                s_delayedRelease.Add(m_internal.drawCall_Rasterize_rootSig);
                m_internal.drawCall_Rasterize_rootSig = nullptr;
            }
        }
        if(!m_internal.drawCall_Rasterize_pso || !m_internal.drawCall_Rasterize_rootSig)
        {

            D3D12_DESCRIPTOR_RANGE rangesVertex[1];

            // _VertexShaderCB
            rangesVertex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            rangesVertex[0].NumDescriptors = 1;
            rangesVertex[0].BaseShaderRegister = 0;
            rangesVertex[0].RegisterSpace = 0;
            rangesVertex[0].OffsetInDescriptorsFromTableStart = 0;

            D3D12_ROOT_PARAMETER rootParams[1];

            rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
            rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
            rootParams[0].DescriptorTable.pDescriptorRanges = rangesVertex;

            // Root desc
            D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
            rootDesc.NumParameters = 1;
            rootDesc.pParameters = rootParams;
            rootDesc.NumStaticSamplers = 0;
            rootDesc.pStaticSamplers = nullptr;
            rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            // Create it
            ID3DBlob* sig = nullptr;
            ID3DBlob* error = nullptr;
            HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &error);
            if (FAILED(hr))
            {
                const char* errorMsg = (error ? (const char*)error->GetBufferPointer() : nullptr);
                if (errorMsg)
                    Context::LogFn(LogLevel::Error, "Could not serialize root signature : %s", errorMsg);
                if (sig) sig->Release();
                if (error) error->Release();
                return false;
            }

            char* sigptr = (char*)sig->GetBufferPointer();
            auto sigsize = sig->GetBufferSize();

            hr = device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&m_internal.drawCall_Rasterize_rootSig));
            if (FAILED(hr))
            {
                const char* errorMsg = (error ? (const char*)error->GetBufferPointer() : nullptr);
                if (errorMsg)
                    Context::LogFn(LogLevel::Error, "Could not create root signature: %s", errorMsg);
                if (sig) sig->Release();
                if (error) error->Release();
                return false;
            }

            if (sig)
                sig->Release();

            if (error)
                error->Release();

            // name the root signature for debuggers
            if (c_debugNames)
                m_internal.drawCall_Rasterize_rootSig->SetName(L"Rasterize");

            D3D_SHADER_MACRO* definesVS = nullptr;

            std::vector<unsigned char> byteCodeVS = DX12Utils::CompileShaderToByteCode_DXC(Context::s_techniqueLocation.c_str(), L"shaders/simpleRaster_VS.hlsl", "VSMain", "vs_6_1", definesVS, c_debugShaders, Context::LogFn);
            if (byteCodeVS.size() == 0)
                return false;

            D3D_SHADER_MACRO* definesPS = nullptr;

            std::vector<unsigned char> byteCodePS = DX12Utils::CompileShaderToByteCode_DXC(Context::s_techniqueLocation.c_str(), L"shaders/simpleRaster_PS.hlsl", "PSMain", "ps_6_1", definesPS, c_debugShaders, Context::LogFn);
            if (byteCodePS.size() == 0)
                return false;

            std::vector<D3D12_INPUT_ELEMENT_DESC> vertexInputLayout;

            // Vertex buffer vertex input layout
            vertexInputLayout.insert(vertexInputLayout.end(), m_input.buffer_VertexBuffer_vertexInputLayout.begin(), m_input.buffer_VertexBuffer_vertexInputLayout.end());

            // Make the PSO desc
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout = { vertexInputLayout.data(), (UINT)vertexInputLayout.size() };
            psoDesc.pRootSignature = m_internal.drawCall_Rasterize_rootSig;
            psoDesc.VS.pShaderBytecode = byteCodeVS.data();
            psoDesc.VS.BytecodeLength = byteCodeVS.size();
            psoDesc.PS.pShaderBytecode = byteCodePS.data();
            psoDesc.PS.BytecodeLength = byteCodePS.size();
            psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
            psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            psoDesc.RasterizerState.DepthClipEnable = TRUE;
            psoDesc.RasterizerState.MultisampleEnable = FALSE;
            psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
            psoDesc.RasterizerState.ForcedSampleCount = 0;
            psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
            psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
            psoDesc.BlendState.IndependentBlendEnable = FALSE;

            psoDesc.BlendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC
            {
                TRUE, FALSE,
                D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP, 7
            };

            psoDesc.BlendState.RenderTarget[1] = D3D12_RENDER_TARGET_BLEND_DESC
            {
                FALSE, FALSE,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP, 15
            };

            psoDesc.BlendState.RenderTarget[2] = D3D12_RENDER_TARGET_BLEND_DESC
            {
                FALSE, FALSE,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP, 15
            };

            psoDesc.BlendState.RenderTarget[3] = D3D12_RENDER_TARGET_BLEND_DESC
            {
                FALSE, FALSE,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP, 15
            };

            psoDesc.BlendState.RenderTarget[4] = D3D12_RENDER_TARGET_BLEND_DESC
            {
                FALSE, FALSE,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP, 15
            };

            psoDesc.BlendState.RenderTarget[5] = D3D12_RENDER_TARGET_BLEND_DESC
            {
                FALSE, FALSE,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP, 15
            };

            psoDesc.BlendState.RenderTarget[6] = D3D12_RENDER_TARGET_BLEND_DESC
            {
                FALSE, FALSE,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP, 15
            };

            psoDesc.BlendState.RenderTarget[7] = D3D12_RENDER_TARGET_BLEND_DESC
            {
                FALSE, FALSE,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP, 15
            };

            psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

            psoDesc.RTVFormats[0] = m_output.texture_Color_Buffer_format;
            psoDesc.NumRenderTargets = 1;

            psoDesc.DepthStencilState.DepthEnable = TRUE;
            psoDesc.DSVFormat = DX12Utils::DSV_Safe_DXGI_FORMAT(m_output.texture_Depth_Buffer_format);
            if (DX12Utils::isStencilDXGI_FORMAT(psoDesc.DSVFormat))
            {
                psoDesc.DepthStencilState.StencilEnable = TRUE;
                psoDesc.DepthStencilState.StencilReadMask = 255;
                psoDesc.DepthStencilState.StencilWriteMask = 255;
                psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
                psoDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
                psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
                psoDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
                psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
                psoDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
                psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
                psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            }
            psoDesc.SampleDesc.Count = 1;
            hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_internal.drawCall_Rasterize_pso));
            if (FAILED(hr))
            {
                Context::LogFn(LogLevel::Error, "Could not create PSO for Rasterize");
                return false;
            }

            // name the PSO for debuggers
            if (c_debugNames)
                m_internal.drawCall_Rasterize_pso->SetName(L"Rasterize");
        }
        return true;
    }
};
