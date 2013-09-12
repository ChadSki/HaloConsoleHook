#pragma once
#include "stdafx.h"

// For exporting functions without name-mangling
#define DllExport extern "C" __declspec( dllexport )

// Our sole export for the time being
DllExport void InjectedMain(const wchar_t * argument);
