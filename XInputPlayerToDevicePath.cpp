//
// WinAPI
// 
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winioctl.h>
#include <Xinput.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>

#pragma comment(lib, "cfgmgr32")

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

	DEVPROPTYPE type = 0;
	ULONG sizeRequired = 0;

	CM_Get_Device_Interface_Property(
		lpFileName,
		&DEVPKEY_Device_InstanceId,
		&type,
		NULL,
		&sizeRequired,
		0
	);

	const PWSTR buffer = (PWSTR)malloc(sizeRequired);

	CM_Get_Device_Interface_Property(
		lpFileName,
		&DEVPKEY_Device_InstanceId,
		&type,
		(PUCHAR)buffer,
		&sizeRequired,
		0
	);

	std::wcout << L"Device Instance ID: " << buffer << std::endl;

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

	std::wcout << "Calling XInputGetCapabilities for User Index 0" << std::endl;

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
