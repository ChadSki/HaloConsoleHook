#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#include <iostream>
#include <TlHelp32.h>
#include <stdlib.h>
#include <string>

#include "HCommonEnsureCleanup.hpp"
#include "Injection.hpp"
