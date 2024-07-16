///////////////////////////////////////////////////////////////////////////////
//         Gigi Rapid Graphics Prototyping and Code Generation Suite         //
//        Copyright (c) 2024 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

// clang-format off
#include "Backends/Shared.h"
#include "TupleCache.h"
#include "Utils.h"
#include "gigicompiler.h"
#include <filesystem>
#include <process.h>
// clang-format on

enum class NodeAction
{
	Init,
	Execute
};

struct VariableStorage
{
public:
	struct Storage
	{
		// in m_variableStorage
		void* value = nullptr;

		// in m_variableStorage
		void* dflt = nullptr;

		size_t size = 0;
	};

	void Clear() { m_storage.Clear(); }

	static void SetFromString(const char* text, size_t count, int* value);
	static void SetFromString(const char* text, size_t count, unsigned int* value);
	static void SetFromString(const char* text, size_t count, float* value);
	static void SetFromString(const char* text, size_t count, bool* value);
	static void SetFromString(const char* text, size_t count, uint16_t* value);

private:
	template<typename T>
	Storage Get(const Variable& variable, size_t count, T* dummy)
	{
		// Get or create variable storage.
		// Allocate double the memory needed because we need to store both the value and the default value
		bool			   newStorage = false;
		std::vector<char>& storage	  = m_storage.GetOrCreate(variable.name, variable.type, newStorage);
		if (newStorage)
			storage.resize(sizeof(T) * count * 2);

		// set up the return object
		Storage ret;
		ret.size  = sizeof(T) * count;
		ret.value = storage.data();
		ret.dflt  = &((char*)ret.value)[ret.size];

		// parse the dflt and set the value to the dflt if it's new storage
		SetFromString(variable.dflt.c_str(), count, (T*)ret.dflt);
		if (newStorage)
			memcpy(ret.value, ret.dflt, ret.size);

		// return the storage object
		return ret;
	}

	static std::string GetAsString(size_t count, int* value);
	static std::string GetAsString(size_t count, unsigned int* value);
	static std::string GetAsString(size_t count, float* value);
	static std::string GetAsString(size_t count, bool* value);
	static std::string GetAsString(size_t count, uint16_t* value);

	template<typename LAMBDA>
	void CallFor_Int(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = int;
		static const size_t TheCount = 1;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Int2(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = int;
		static const size_t TheCount = 2;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Int3(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = int;
		static const size_t TheCount = 3;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Int4(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = int;
		static const size_t TheCount = 4;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Uint(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = unsigned int;
		static const size_t TheCount = 1;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Uint2(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = unsigned int;
		static const size_t TheCount = 2;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Uint3(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = unsigned int;
		static const size_t TheCount = 3;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Uint4(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = unsigned int;
		static const size_t TheCount = 4;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Float(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = float;
		static const size_t TheCount = 1;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Float2(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = float;
		static const size_t TheCount = 2;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Float3(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = float;
		static const size_t TheCount = 3;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Float4(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = float;
		static const size_t TheCount = 4;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Bool(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = bool;
		static const size_t TheCount = 1;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Float4x4(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = float;
		static const size_t TheCount = 16;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Uint_16(const Variable& variable, const LAMBDA& lambda)
	{
		using TheType				 = uint16_t;
		static const size_t TheCount = 1;

		Storage storage = Get(variable, TheCount, (TheType*)nullptr);
		lambda(TheCount, (TheType*)storage.value);
	}

	template<typename LAMBDA>
	void CallFor_Count(const Variable& variable, const LAMBDA& lambda)
	{
	}

	template<typename LAMBDA>
	void CallForVariable(const Variable& variable, const LAMBDA& lambda)
	{
		switch (variable.type)
		{
#include "external/df_serialize/_common.h"
#define ENUM_ITEM(_NAME, _DESCRIPTION) \
	case DataFieldType::_NAME: CallFor_##_NAME(variable, lambda); break;
// clang-format off
#include "external/df_serialize/_fillunsetdefines.h"
#include "Schemas/DataFieldTypes.h"
// clang-format on
		}
	}

public:
	void SetValueFromString(const Variable& variable, const char* textValue)
	{
		CallForVariable(variable,
			[&](size_t count, auto* value)
			{
				SetFromString(textValue, count, value);
			});
	}

	std::string GetValueAsString(const Variable& variable)
	{
		std::string ret;
		CallForVariable(variable,
			[&](size_t count, auto* value)
			{
				ret = GetAsString(count, value);
			});
		return ret;
	}

	Storage Get(const Variable& variable)
	{
		Storage ret;
		CallForVariable(variable,
			[&](size_t count, auto* value)
			{
				ret = Get(variable, count, value);
			});
		return ret;
	}

private:
	TupleCache<std::vector<char>, std::string, DataFieldType> m_storage;
};

template<typename TRuntimeTypes>
class IGigiInterpreter
{
public:
	GigiCompileResult Compile(const char* fileName, void (*PostLoad)(RenderGraph&))
	{
		OnPreCompile();

		m_renderGraph = RenderGraph(); // clear out any stuff that may be there in the render graph already.

		// Get a Gigi temporary directory based on the process ID so multiple viewers can run at once
		m_tempDirectory = std::filesystem::temp_directory_path().string();
		m_tempDirectory += "Gigi\\";
		char pid[256];
		sprintf_s(pid, "%i", _getpid());
		m_tempDirectory += pid;
		m_tempDirectory += "\\";

		// Remove everything aready there, to prevent stale things interfering
		// Then, make sure the directory is created
		std::filesystem::remove_all(m_tempDirectory.c_str());
		std::filesystem::create_directories(m_tempDirectory.c_str());

		m_compileResult = GigiCompile(GigiBuildFlavor::Interpreter_Interpreter, fileName, m_tempDirectory.c_str(), PostLoad, &m_renderGraph, false);
		if (m_compileResult != GigiCompileResult::OK)
			return m_compileResult;

		// Make runtime storage for variables
		CreateVariableStorage(m_renderGraph);

		OnCompileOK();

#include "external/df_serialize/_common.h"
#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION) m_##_TYPE##_RuntimeData.Clear();
// clang-format off
#include "external/df_serialize/_fillunsetdefines.h"
#include "Schemas/RenderGraphNodesVariant.h"
// clang-format on

		// Do initialization for each node
		for (int nodeIndex : m_renderGraph.flattenedNodeList)
		{
			const RenderGraphNode& node = m_renderGraph.nodes[nodeIndex];

			switch (node._index)
			{
#include "external/df_serialize/_common.h"
#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION)                                                                                            \
	case RenderGraphNode::c_index_##_NAME:                                                                                                            \
	{                                                                                                                                                 \
		if (!OnNodeAction(node.##_NAME, m_##_TYPE##_RuntimeData.GetOrCreate(node.##_NAME.name), NodeAction::Init))                                    \
		{                                                                                                                                             \
			m_logFn(LogLevel::Error, "Error during IGigiInterpreter::Compile OnNodeAction(Init) in node %s (" #_NAME ")", node.##_NAME.name.c_str()); \
			return GigiCompileResult::InterpreterError;                                                                                               \
		}                                                                                                                                             \
		break;                                                                                                                                        \
	}
// clang-format off
#include "external/df_serialize/_fillunsetdefines.h"
#include "Schemas/RenderGraphNodesVariant.h"
// clang-format on
			}
		}

		return GigiCompileResult::OK;
	}

	// Default generic type operations
	template<typename T>
	void DoOp(T& A, T& B, T& dest, SetVariableOperator op)
	{
		switch (op)
		{
			case SetVariableOperator::Add: dest = A + B; break;
			case SetVariableOperator::Subtract: dest = A - B; break;
			case SetVariableOperator::Multiply: dest = A * B; break;
			case SetVariableOperator::Divide:
			{
				dest = (B != 0) ? A / B : 0;
				break;
			}
			case SetVariableOperator::Modulo: dest = A % B; break;

			case SetVariableOperator::BitwiseOr: dest = A | B; break;
			case SetVariableOperator::BitwiseAnd: dest = A & B; break;
			case SetVariableOperator::BitwiseXor: dest = A ^ B; break;
			case SetVariableOperator::BitwiseNot: dest = ~A; break;

			case SetVariableOperator::PowerOf2GE:
			{
				float f = std::log2(float(A));
				f = std::ceilf(f);
				dest = (T)std::pow(2.0f, f);
				break;
			}

			case SetVariableOperator::Noop: dest = A; break;
		}
	}

	// floating point operation specialization
	template<>
	void DoOp<float>(float& A, float& B, float& dest, SetVariableOperator op)
	{
		switch (op)
		{
			case SetVariableOperator::Add: dest = A + B; break;
			case SetVariableOperator::Subtract: dest = A - B; break;
			case SetVariableOperator::Multiply: dest = A * B; break;
			case SetVariableOperator::Divide: dest = A / B; break;
			case SetVariableOperator::Modulo:
				dest = std::fmod(A, B);
				break;

				// invalid operation
				/*
				case SetVariableOperator::BitwiseOr: dest = A | B; break;
				case SetVariableOperator::BitwiseAnd: dest = A & B; break;
				case SetVariableOperator::BitwiseXor: dest = A ^ B; break;
				case SetVariableOperator::BitwiseNot: dest = ~A; break;
				*/

			case SetVariableOperator::PowerOf2GE:
			{
				float f = std::log2(A);
				f = std::ceilf(f);
				dest = std::pow(2.0f, f);
				break;
			}

			case SetVariableOperator::Noop: dest = A; break;
		}
	}

	// boolean operation specialization
	template<>
	void DoOp<bool>(bool& A, bool& B, bool& dest, SetVariableOperator op)
	{
		switch (op)
		{
				/*
				case SetVariableOperator::Add: dest = A + B; break;
				case SetVariableOperator::Subtract: dest = A - B; break;
				case SetVariableOperator::Multiply: dest = A * B; break;
				case SetVariableOperator::Divide: dest = A / B; break;
				case SetVariableOperator::Modulo: dest = A % B; break;
					*/

			case SetVariableOperator::BitwiseOr: dest = A || B; break;
			case SetVariableOperator::BitwiseAnd: dest = A && B; break;
			case SetVariableOperator::BitwiseXor: dest = A ^ B; break;
			case SetVariableOperator::BitwiseNot: dest = !A; break;

			case SetVariableOperator::Noop: dest = A; break;
		}
	}

	template<typename T>
	void DoSetVarOperation(const SetVariable& setVar, void* ABytes, void* BBytes, void* destBytes, int componentCount, int componentByteCount = sizeof(T))
	{
		// Get the typed pointers
		T* A	= (T*)ABytes;
		T* B	= (T*)BBytes;
		T* dest = (T*)destBytes;

		std::vector<char> ABuffer;
		std::vector<char> BBuffer;

		// Set the A value from texture size, buffer count, or literal, if not previously set by a variable
		if (setVar.ANode.textureNode)
		{
			const auto& runtimeResourceData = m_RenderGraphNode_Resource_Texture_RuntimeData.GetOrCreate(setVar.ANode.textureNode->name);

			char sizeAsString[256];
			sprintf_s(sizeAsString, "%u, %u, %u", runtimeResourceData.m_size[0], runtimeResourceData.m_size[1], runtimeResourceData.m_size[2]);

			ABuffer.resize(3 * componentByteCount);
			A = (T*)ABuffer.data();

			VariableStorage::SetFromString(sizeAsString, 3, A);
		}
		else if (setVar.ANode.bufferNode)
		{
			const auto& runtimeResourceData = m_RenderGraphNode_Resource_Buffer_RuntimeData.GetOrCreate(setVar.ANode.bufferNode->name);

			char sizeAsString[256];
			sprintf_s(sizeAsString, "%u", runtimeResourceData.m_count);

			ABuffer.resize(1 * componentByteCount);
			A = (T*)ABuffer.data();

			VariableStorage::SetFromString(sizeAsString, 1, A);
		}
		else if (setVar.AVar.variableIndex == -1)
		{
			VariableStorage::SetFromString(setVar.ALiteral.c_str(), componentCount, A);
		}

		// Set the B value from texture size, buffer count, or literal, if not previously set by a variable
		if (setVar.BNode.textureNode)
		{
			const auto& runtimeResourceData = m_RenderGraphNode_Resource_Texture_RuntimeData.GetOrCreate(setVar.BNode.textureNode->name);

			char sizeAsString[256];
			sprintf_s(sizeAsString, "%u, %u, %u", runtimeResourceData.m_size[0], runtimeResourceData.m_size[1], runtimeResourceData.m_size[2]);

			BBuffer.resize(3 * componentByteCount);
			B = (T*)BBuffer.data();

			VariableStorage::SetFromString(sizeAsString, 3, B);
		}
		else if (setVar.BNode.bufferNode)
		{
			const auto& runtimeResourceData = m_RenderGraphNode_Resource_Buffer_RuntimeData.GetOrCreate(setVar.BNode.bufferNode->name);

			char sizeAsString[256];
			sprintf_s(sizeAsString, "%u", runtimeResourceData.m_count);

			BBuffer.resize(1 * componentByteCount);
			B = (T*)BBuffer.data();

			VariableStorage::SetFromString(sizeAsString, 1, B);
		}
		else if (setVar.BVar.variableIndex == -1)
			VariableStorage::SetFromString(setVar.BLiteral.c_str(), componentCount, B);

		// Limit the values involved to the appropriate index
		if (setVar.destinationIndex != -1)
		{
			componentCount = 1;
			dest = &dest[setVar.destinationIndex];
		}

		if (setVar.AVarIndex != -1)
		{
			componentCount = 1;
			A = &A[setVar.AVarIndex];
		}

		if (setVar.BVarIndex != -1)
		{
			componentCount = 1;
			B = &B[setVar.BVarIndex];
		}

		// Do the operation on each component
		for (int i = 0; i < componentCount; ++i)
			DoOp(A[i], B[i], dest[i], setVar.op);
	}

	void ExecuteSetvar(const SetVariable& setVar)
	{
		DataFieldType			type	 = m_renderGraph.variables[setVar.destination.variableIndex].type;
		DataFieldTypeInfoStruct typeInfo = DataFieldTypeInfo(type);

		void* destBytes = GetRuntimeVariable(setVar.destination.variableIndex).storage.value;
		void* ABytes	= (setVar.AVar.variableIndex != -1) ? GetRuntimeVariable(setVar.AVar.variableIndex).storage.value : nullptr;
		void* BBytes	= (setVar.BVar.variableIndex != -1) ? GetRuntimeVariable(setVar.BVar.variableIndex).storage.value : nullptr;

		std::vector<char> ABuffer;
		std::vector<char> BBuffer;

		if (!ABytes)
		{
			ABuffer.resize(typeInfo.typeBytes);
			ABytes = ABuffer.data();
		}

		if (!BBytes)
		{
			BBuffer.resize(typeInfo.typeBytes);
			BBytes = BBuffer.data();
		}

		switch (typeInfo.componentType2)
		{
			case DataFieldType::Bool:
			{
				DoSetVarOperation<bool>(setVar, ABytes, BBytes, destBytes, typeInfo.componentCount, 4);
				break;
			}
			case DataFieldType::Int:
			{
				DoSetVarOperation<int>(setVar, ABytes, BBytes, destBytes, typeInfo.componentCount);
				break;
			}
			case DataFieldType::Uint_16:
			{
				DoSetVarOperation<uint16_t>(setVar, ABytes, BBytes, destBytes, typeInfo.componentCount);
				break;
			}
			case DataFieldType::Uint:
			{
				DoSetVarOperation<uint32_t>(setVar, ABytes, BBytes, destBytes, typeInfo.componentCount);
				break;
			}
			case DataFieldType::Float:
			{
				DoSetVarOperation<float>(setVar, ABytes, BBytes, destBytes, typeInfo.componentCount);
				break;
			}
		}
	}

	template<typename T>
	bool DoComparison(T& A, T& B, ConditionComparison op)
	{
		switch (op)
		{
			case ConditionComparison::IsTrue: return A != T(0);
			case ConditionComparison::IsFalse: return A == T(0);
			case ConditionComparison::Equals: return A == B;
			case ConditionComparison::NotEquals: return A != B;
			case ConditionComparison::LT: return A < B;
			case ConditionComparison::LTE: return A <= B;
			case ConditionComparison::GT: return A > B;
			case ConditionComparison::GTE: return A >= B;
		}
		return false;
	}

	bool StringBeginsWithCaseInsensitive(const char* hayStack, const char* needle)
	{
		if (!hayStack || !needle)
			return false;

		auto ToLower = [](char c) -> char
		{
			if (c >= 'A' && c <= 'Z')
				return c - 'A' + 'a';
			else
				return c;
		};

		int i = 0;
		while (hayStack[i] && needle[i])
		{
			if (ToLower(hayStack[i]) != ToLower(needle[i]))
				return false;
			i++;
		}

		return needle[i] == 0;
	}

	// returns -1 if there was a problem
	int EnumLabelToValue(const Enum& e, const char* label)
	{
		// If the literal contain "EnumName::", then skip that part
		const char* literalValue = label;
		std::string enumPrefix	 = e.originalName + "::";
		if (StringBeginsWithCaseInsensitive(literalValue, enumPrefix.c_str()))
			literalValue += enumPrefix.length();

		// Convert from enum name to integer value
		bool validEnum = false;
		for (size_t i = 0; i < e.items.size(); ++i)
		{
			if (!_stricmp(literalValue, e.items[i].label.c_str()))
				return (int)i;
		}
		return -1;
	}

	bool EvaluateConditionTyped_Enum(const Condition& condition, void* ABytes, void* BBytes, int componentCount)
	{
		// Get the typed pointers
		int* A = (int*)ABytes;
		int* B = (int*)BBytes;

		// Get the integer value from the enum label
		const Enum& e = m_renderGraph.enums[m_renderGraph.variables[condition.variable1Index].enumIndex];
		*B			  = EnumLabelToValue(e, condition.value2.c_str());

		// Do the operation on each component
		bool ret = true;
		for (int i = 0; i < componentCount; ++i)
			ret = ret && DoComparison(A[i], B[i], condition.comparison);
		return ret;
	}

	template<typename T>
	bool EvaluateConditionTyped(const Condition& condition, void* ABytes, void* BBytes, int componentCount)
	{
		// Get the typed pointers
		T* A = (T*)ABytes;
		T* B = (T*)BBytes;

		// Read from literals as needed
		if (condition.variable2Index == -1)
			VariableStorage::SetFromString(condition.value2.c_str(), componentCount, B);

		// Do the operation on each component
		bool ret = true;
		for (int i = 0; i < componentCount; ++i)
			ret = ret && DoComparison(A[i], B[i], condition.comparison);
		return ret;
	}

	bool IsConditional(const Condition& condition)
	{
		return condition.comparison != ConditionComparison::Count || condition.alwaysFalse;
	}

	bool EvaluateCondition(const Condition& condition)
	{
		if (condition.alwaysFalse)
			return false;

		if (condition.variable1Index == -1 || condition.comparison == ConditionComparison::Count)
			return true;

		DataFieldType			type	 = m_renderGraph.variables[condition.variable1Index].type;
		DataFieldTypeInfoStruct typeInfo = DataFieldTypeInfo(type);

		void* ABytes = GetRuntimeVariable(condition.variable1Index).storage.value;

		void*			  BBytes = nullptr;
		std::vector<char> BBuffer;
		if (condition.variable2Index != -1)
		{
			BBytes = GetRuntimeVariable(condition.variable2Index).storage.value;
		}
		else
		{
			BBuffer.resize(typeInfo.typeBytes);
			BBytes = BBuffer.data();
		}

		if (type == DataFieldType::Bool)
		{
			return EvaluateConditionTyped<bool>(condition, ABytes, BBytes, typeInfo.componentCount);
		}
		else
		{
			switch (typeInfo.componentType)
			{
				case DataFieldComponentType::_int:
				{
					// If variable 1 is an enum, and the RHS is a literal, set as an enm
					if (m_renderGraph.variables[condition.variable1Index].enumIndex != -1 && condition.variable2Index == -1)
						return EvaluateConditionTyped_Enum(condition, ABytes, BBytes, typeInfo.componentCount);
					else
						return EvaluateConditionTyped<int>(condition, ABytes, BBytes, typeInfo.componentCount);
				}
				case DataFieldComponentType::_uint16_t: return EvaluateConditionTyped<uint16_t>(condition, ABytes, BBytes, typeInfo.componentCount);
				case DataFieldComponentType::_uint32_t: return EvaluateConditionTyped<uint32_t>(condition, ABytes, BBytes, typeInfo.componentCount);
				case DataFieldComponentType::_float: return EvaluateConditionTyped<float>(condition, ABytes, BBytes, typeInfo.componentCount);
			}
		}

		return true;
	}

	void ExecuteSetVars(bool beforeExecution)
	{
		for (const SetVariable& setVar : m_renderGraph.setVars)
		{
			if (setVar.destination.variableIndex == -1 || setVar.setBefore != beforeExecution)
				continue;

			if (!EvaluateCondition(setVar.condition))
				continue;

			ExecuteSetvar(setVar);
		}
	}

	bool Execute()
	{
		if (m_compileResult != GigiCompileResult::OK)
			return true;

		ExecuteSetVars(true);

		for (int nodeIndex : m_renderGraph.flattenedNodeList)
		{
			RenderGraphNode& node = m_renderGraph.nodes[nodeIndex];

			switch (node._index)
			{
#include "external/df_serialize/_common.h"
#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION)                                                            \
	case RenderGraphNode::c_index_##_NAME:                                                                            \
	{                                                                                                                 \
		if (!OnNodeAction(node.##_NAME, m_##_TYPE##_RuntimeData.GetOrCreate(node.##_NAME.name), NodeAction::Execute)) \
			return false;                                                                                             \
		break;                                                                                                        \
	}
// clang-format off
#include "external/df_serialize/_fillunsetdefines.h"
#include "Schemas/RenderGraphNodesVariant.h"
// clang-format on
			}
		}

		ExecuteSetVars(false);

		return true;
	}

	void Clear()
	{
		m_compileResult = GigiCompileResult::NotCompiledYet;
		m_variableStorage.Clear();
		m_runtimeVariables.clear();
	}

	virtual void ShowUI() {}
	virtual void OnPreCompile() {}
	virtual void OnCompileOK() {}

	void SetLogFn(LogFn logFn)
	{
		m_logFn = logFn;
	}

	struct RuntimeVariable
	{
		// in the render graph
		const Variable* variable = nullptr;

		VariableStorage::Storage storage;
	};

	const RuntimeVariable& GetRuntimeVariable(int index) const
	{
		return m_runtimeVariables[index];
	}

	int GetRuntimeVariableIndex(const char* name) const
	{
		int index = -1;
		for (const auto& rtVar : m_runtimeVariables)
		{
			index++;
			if (!strcmp(rtVar.variable->name.c_str(), name))
				return index;
		}

		return -1;
	}

	const int GetRuntimeVariableCount() const
	{
		return (int)m_runtimeVariables.size();
	}

	std::string GetRuntimeVariableValueAsString(int index)
	{
		return m_variableStorage.GetValueAsString(*m_runtimeVariables[index].variable);
	}

	void SetRuntimeVariableFromString(int index, const char* textValue)
	{
		// enums can be set by either enum string, or integer value.
		// Try the enum string first, but fall through to integer handling if not handled.
		if (m_runtimeVariables[index].variable->enumIndex != -1)
		{
			const Enum& e		 = m_renderGraph.enums[m_runtimeVariables[index].variable->enumIndex];
			int			valueInt = EnumLabelToValue(e, textValue);
			if (valueInt >= 0)
			{
				char valueIntString[256];
				sprintf_s(valueIntString, "%i", valueInt);
				m_variableStorage.SetValueFromString(*m_runtimeVariables[index].variable, valueIntString);
				return;
			}
		}

		m_variableStorage.SetValueFromString(*m_runtimeVariables[index].variable, textValue);
	}

	void SetRuntimeVariableToDflt(int index)
	{
		memcpy(m_runtimeVariables[index].storage.value, m_runtimeVariables[index].storage.dflt, m_runtimeVariables[index].storage.size);
	}

	const RenderGraph& GetRenderGraph() const
	{
		return m_renderGraph;
	}

public:
#include "external/df_serialize/_common.h"
#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION)                                                  \
	const typename TRuntimeTypes::_TYPE& GetRuntimeNodeData_##_TYPE##(const char* name, bool& exists) const \
	{                                                                                                       \
		return m_##_TYPE##_RuntimeData.Get(name, exists);                                                   \
	}                                                                                                       \
	typename TRuntimeTypes::_TYPE& GetRuntimeNodeData_##_TYPE##(const char* name, bool& exists)             \
	{                                                                                                       \
		return m_##_TYPE##_RuntimeData.Get(name, exists);                                                   \
	}                                                                                                       \
	const typename TRuntimeTypes::_TYPE& GetRuntimeNodeData_##_TYPE##(const char* name) const               \
	{                                                                                                       \
		bool exists = false;                                                                                \
		return m_##_TYPE##_RuntimeData.Get(name, exists);                                                   \
	}                                                                                                       \
	typename TRuntimeTypes::_TYPE& GetRuntimeNodeData_##_TYPE##(const char* name)                           \
	{                                                                                                       \
		bool exists = false;                                                                                \
		return m_##_TYPE##_RuntimeData.Get(name, exists);                                                   \
	}
// clang-format off
#include "external/df_serialize/_fillunsetdefines.h"
#include "Schemas/RenderGraphNodesVariant.h"
// clang-format on

	template<typename LAMBDA>
	void RuntimeNodeDataLambda(const RenderGraphNode& node, const LAMBDA& lambda)
	{
		switch (node._index)
		{
#include "external/df_serialize/_common.h"
#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION)                                                     \
	case RenderGraphNode::c_index_##_NAME:                                                                     \
	{                                                                                                          \
		const auto& typedNode		  = node.##_NAME;                                                          \
		bool		runtimeDataExists = false;                                                                 \
		auto&		runtimeData		  = GetRuntimeNodeData_##_TYPE(typedNode.name.c_str(), runtimeDataExists); \
		lambda(typedNode, runtimeDataExists ? &runtimeData : nullptr);                                         \
		break;                                                                                                 \
	}
// clang-format off
#include "external/df_serialize/_fillunsetdefines.h"
#include "Schemas/RenderGraphNodesVariant.h"
// clang-format on
		}
	}

	template<typename LAMBDA>
	void RuntimeNodeDataLambda(const RenderGraphNode& node, const LAMBDA& lambda) const
	{
		switch (node._index)
		{
#include "external/df_serialize/_common.h"
#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION)                                                     \
	case RenderGraphNode::c_index_##_NAME:                                                                     \
	{                                                                                                          \
		const auto& typedNode		  = node.##_NAME;                                                          \
		bool		runtimeDataExists = false;                                                                 \
		const auto& runtimeData		  = GetRuntimeNodeData_##_TYPE(typedNode.name.c_str(), runtimeDataExists); \
		lambda(typedNode, runtimeDataExists ? &runtimeData : nullptr);                                         \
		break;                                                                                                 \
	}
// clang-format off
#include "external/df_serialize/_fillunsetdefines.h"
#include "Schemas/RenderGraphNodesVariant.h"
// clang-format on
		}
	}

	std::string GetTempDirectory() const
	{
		return m_tempDirectory;
	}

protected:
#include "external/df_serialize/_common.h"
#define VARIANT_TYPE(_TYPE, _NAME, _DEFAULT, _DESCRIPTION)                                                                                                         \
	virtual bool										   OnNodeAction(const _TYPE& node, typename TRuntimeTypes::_TYPE& runtimeData, NodeAction nodeAction) = 0; \
	TupleCache<typename TRuntimeTypes::_TYPE, std::string> m_##_TYPE##_RuntimeData;
// clang-format off
#include "external/df_serialize/_fillunsetdefines.h"
#include "Schemas/RenderGraphNodesVariant.h"
// clang-format on

protected:
	struct VariableDataStorageKey
	{
		VariableDataStorageKey()
		{
		}

		VariableDataStorageKey(const std::string& _name, const DataFieldType& _type)
		: name(_name)
		, type(_type)
		{
		}

		std::string	  name;
		DataFieldType type = DataFieldType::Count;

		bool operator==(const VariableDataStorageKey& rhs) const
		{
			return name == rhs.name && type == rhs.type;
		}

		size_t operator()(const VariableDataStorageKey& key) const
		{
			return std::hash<std::string>()(key.name) ^ std::hash<int>()((int)key.type);
		}
	};

private:
	void CreateVariableStorage(const RenderGraph& renderGraph)
	{
		// Make runtime storage for variables
		m_runtimeVariables.clear();
		m_runtimeVariables.resize(renderGraph.variables.size());
		for (size_t i = 0; i < renderGraph.variables.size(); ++i)
		{
			m_runtimeVariables[i].variable = &renderGraph.variables[i];
			m_runtimeVariables[i].storage  = m_variableStorage.Get(*m_runtimeVariables[i].variable);
		}
	}

protected:
	GigiCompileResult m_compileResult = GigiCompileResult::NotCompiledYet;
	RenderGraph		  m_renderGraph;
	std::string		  m_tempDirectory;

	VariableStorage				 m_variableStorage;
	std::vector<RuntimeVariable> m_runtimeVariables;
	LogFn						 m_logFn = [](LogLevel level, const char* msg, ...) {};
};
