#pragma once

#include "technique.h"

namespace UnitTests
{

    static PyMethodDef pythonModuleMethods[] = {
        {nullptr, nullptr, 0, nullptr}
    };

    static PyModuleDef pythonModule = {
        PyModuleDef_HEAD_INIT, "UnitTests", NULL, -1, pythonModuleMethods,
        NULL, NULL, NULL, NULL
    };

    PyObject* CreateModule()
    {
        PyObject* module = PyModule_Create(&pythonModule);
        return module;
    }
};
