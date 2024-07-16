///////////////////////////////////////////////////////////////////////////////
//         Gigi Rapid Graphics Prototyping and Code Generation Suite         //
//        Copyright (c) 2024 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

// List the build flavors here
// Makes an enum called GigiBuildFlavor that concats the 2 fields with an underscore between them.
// These are used by GigiCompiler, GigiCompilerLib and GigiEdit

GIGI_BUILD_FLAVOR(DX12, Module, false)
GIGI_BUILD_FLAVOR(DX12, Application, false)
//GIGI_BUILD_FLAVOR(DX12, HeadlessApplication, false)
GIGI_BUILD_FLAVOR(Interpreter, Interpreter, true)

/*
#define GIGI_BUILD_FLAVOR(BACKEND, FLAVOR, INTERNAL)

BACKEND - the name of the backend the flavor is for.  From BackendList.h

FLAVOR - The flavor. Will be shown as text.

INTERNAL - if true, will not be shown in UI, and will not be accepted from the command line.

*/