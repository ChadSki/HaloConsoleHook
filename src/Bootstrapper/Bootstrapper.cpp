// Copyright (C) 2013 urbanyoung
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "stdafx.h"

//
//-------------------------------------------------------------------------------------------------
// Memory Functions
//

// Writes to process memory
BOOL WriteBytes(DWORD dwAddress, LPVOID lpBuffer, DWORD dwCount)
{
	BOOL bResult = TRUE;

	HANDLE hProcess = GetCurrentProcess();
	LPVOID lpAddress = UlongToPtr(dwAddress);

	DWORD dwOldProtect;
	DWORD dwNewProtect = PAGE_EXECUTE_READWRITE;

	bResult &= VirtualProtect(lpAddress, dwCount, dwNewProtect, &dwOldProtect);		// Allow read/write access
	bResult &= WriteProcessMemory(hProcess, lpAddress, lpBuffer, dwCount, NULL);	// Write to process memory
	bResult &= VirtualProtect(lpAddress, dwCount, dwOldProtect, &dwNewProtect);		// Restore original access
	bResult &= FlushInstructionCache(hProcess, lpAddress, dwCount);					// Update instruction cache

	return bResult;
}

// Finds all locations of a signature
std::vector<DWORD> FindSignature(LPBYTE lpBuffer, DWORD dwBufferSize, LPBYTE lpSignature, DWORD dwSignatureSize, LPBYTE lpWildCards = 0)
{
	// List of locations of the signature
	std::vector<DWORD> addresses;

	// Loop through each byte in the buffer
	for (DWORD i = 0; i < dwBufferSize; ++i)
	{
		bool bFound = true;

		// Loop through each byte in the signature
		for (DWORD j = 0; j < dwSignatureSize; ++j)
		{
			// Check if the index overruns the buffer
			if (i + j >= dwBufferSize)
			{
				bFound = false;
				break;
			}

			// Check if wild cards are used
			if (lpWildCards)
			{
				// Check if the buffer does not equal the signature and a wild card is not set
				if (lpBuffer[i + j] != lpSignature[j] && !lpWildCards[j])
				{
					bFound = false;
					break;
				}
			}
			else
			{
				// Check if the buffer does not equal the signature
				if (lpBuffer[i + j] != lpSignature[j])
				{
					bFound = false;
					break;
				}
			}
		}

		// Check if the signature was found and add it to the address list
		if (bFound)
			addresses.push_back(i);
	}

	// Return the list of locations of the signature
	return addresses;
}

// Finds the location of a signature
DWORD FindAddress(LPBYTE lpBuffer, DWORD dwBufferSize, LPBYTE lpSignature, DWORD dwSignatureSize, LPBYTE lpWildCards = 0, DWORD dwIndex = 0, DWORD dwOffset = 0)
{
	DWORD dwAddress = 0;

	// Find all locations of the signature
	std::vector<DWORD> addresses = FindSignature(lpBuffer, dwBufferSize, lpSignature, dwSignatureSize, lpWildCards);

	// Return the requested address
	if (addresses.size() - 1 >= dwIndex)
		dwAddress = (DWORD)lpBuffer + addresses[dwIndex] + dwOffset;

	return dwAddress;
}

//
//-------------------------------------------------------------------------------------------------
// Halo-specific code
//

char* histBuff;
UINT16* histBuffLen;

/* Ensures that your custom command is added to the console's history (up/down arrows)
 * should the user want to use it again without retyping.
 */
void bufferCommand(char* input)
{
    // Command buffer is a fixed array, 8*255 bytes
    *histBuffLen = (*histBuffLen + 1) % 8;
    char *location = (*histBuffLen * 255) + histBuff;
    strcpy_s(location, 255, input);
}

/* All console commands entered by the player flow through this function.
 * If the command matches one of HAC's, handle it and alter the command
 * buffer to prevent Halo from displaying a 'command not found' error.
 */
void ConsoleRead(char* input)
{
    // set to true if handled a command, otherwise leave false to pass through to Halo
    bool processed = false;

    // TODO Handle commands here

    if(processed) {
        bufferCommand(input);
        input[0] = 0x3B; // prevent hooked function from processing
    }
}

//
//-------------------------------------------------------------------------------------------------
// Code Caves
//

void _declspec(naked) ConsoleReadCC()
{
    __asm {

        pushfd
        pushad
        push edi

        call ConsoleRead

        add esp, 4
        popad
        popfd

        //Original
        mov al, [edi]
        sub esp, 0x00000500
        jmp ConsoleRead // Where should this go?
    }
}

// Creates a code cave to a function at a specific address
BOOL CreateCodeCave(DWORD dwAddress, BYTE cbSize, VOID (*pFunction)())
{
	BOOL bResult = TRUE;

	if (cbSize < 5)
		return FALSE;

	// Calculate the offset from the function to the address
	DWORD dwOffset = PtrToUlong(pFunction) - dwAddress - 5;

	// Construct the call instruction to the offset
	BYTE patch[0xFF] = {0x90};
	patch[0] = 0xE8;
	memcpy(patch + 1, &dwOffset, sizeof(dwAddress));

	// Write the code cave to the address
	bResult &= WriteBytes(dwAddress, patch, cbSize);

	return bResult;
}

//
//-------------------------------------------------------------------------------------------------
//

DllExport void InjectedMain(const wchar_t * argument)
{
    // Get address of code section
	LPBYTE lpModule = (LPBYTE)GetModuleHandle(0);
	DWORD dwOffsetToPE = *(DWORD*)(lpModule + 0x3C);
	DWORD dwSizeOfCode = *(DWORD*)(lpModule + dwOffsetToPE + 0x1C);
	DWORD dwBaseOfCode = *(DWORD*)(lpModule + dwOffsetToPE + 0x2C);
	LPBYTE lpCode = (LPBYTE)(lpModule + dwBaseOfCode);

    // Find the address of histBuff
	BYTE histBuffSig[] = {0x8D, 0x64, 0x24, 0x00, 0x8A, 0x0A, 0x42, 0x88, 0x08, 0x40, 0x3A, 0xCB, 0x75};
	DWORD pHistBuff = FindAddress(lpCode, dwSizeOfCode, histBuffSig, sizeof(histBuffSig)) - 0x5;
    histBuff = (char*)*(UINT32*)pHistBuff;

    // Find the address of histBuffLen
    BYTE histBuffLenSig[] = {0x0F, 0xBF, 0xC0, 0x69, 0xC0, 0xFF, 0x00, 0x00, 0x00, 0x8B, 0xD7, 0x05};
	DWORD pHistBuffLen = FindAddress(lpCode, dwSizeOfCode, histBuffSig, sizeof(histBuffLenSig)) - 0x5;
    histBuffLen = (UINT16*)*(UINT32*)pHistBuffLen;

    // Find the address of the histBuff and histBuffLen
	BYTE sig[] = {0x8D, 0x64, 0x24, 0x00, 0x8A, 0x0A, 0x42, 0x88, 0x08, 0x40, 0x3A, 0xCB, 0x75};
	DWORD cc_onload = FindAddress(lpCode, dwSizeOfCode, sig, sizeof(sig));

    DWORD cc_consoleread = 0x4C6C40;

	// Check if the signature was found and create the code cave
	if (cc_onload)
        CreateCodeCave(cc_consoleread, 5, ConsoleReadCC);
}
