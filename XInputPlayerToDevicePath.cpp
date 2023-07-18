//
// WinAPI
// 
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winioctl.h>
#include <Xinput.h>

//
// Detours
// 
#include <detours/detours.h>

//
// STL
// 
#include <iostream>


static decltype(CreateFileW)* real_CreateFileW = CreateFileW;

typedef DWORD(WINAPI* XInputGetCapabilities_t)(DWORD, DWORD, XINPUT_CAPABILITIES*);
static XInputGetCapabilities_t pGetCapabilities = NULL;


//
// Detoured CreateFileW
// 
HANDLE WINAPI DetourCreateFileW(
	LPCWSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile
)
{
	std::wcout << L"Device path: " << lpFileName << std::endl;

	const auto handle = real_CreateFileW(
		lpFileName,
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile
	);

	return handle;
}


int main()
{
	// 
	// establish hooks
	// 
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach((void**)&real_CreateFileW, DetourCreateFileW);
	DetourTransactionCommit();

	//
	// Get XInputGetCapabilities function
	// 
	pGetCapabilities = (XInputGetCapabilities_t)GetProcAddress(LoadLibrary(L"xinput1_4"), "XInputGetCapabilities");

	XINPUT_CAPABILITIES caps = { 0 };

	//
	// This call will implicitly provoke the CreateFile call we hooked earlier within the logic of XInputX_X.dll
	// 
	const auto ret = pGetCapabilities(0, XINPUT_FLAG_GAMEPAD, &caps);

	//
	// Remove hooks
	// 
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach((void**)&real_CreateFileW, DetourCreateFileW);
	DetourTransactionCommit();

	getchar();
}
