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
#include <map>


static decltype(CreateFileW)* real_CreateFileW = CreateFileW;

typedef DWORD(WINAPI* XInputGetCapabilities_t)(DWORD, DWORD, XINPUT_CAPABILITIES*);
static XInputGetCapabilities_t pGetCapabilities = NULL;

//
// Crude global state keeping helpers
// 
static DWORD G_CurrentUserIndex = 0;
static std::map<DWORD, std::wstring> G_MapUserIndexDeviceInstanceId
{
	{0, L"<not connected>"},
	{ 1, L"<not connected>" },
	{ 2, L"<not connected>" },
	{ 3, L"<not connected>" },
};


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
	//std::wcout << "Looking up User Index " << std::dec << G_CurrentUserIndex << std::endl;

	//std::wcout << L"Device path: " << lpFileName << std::endl;

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

	//std::wcout << L"Device Instance ID: " << buffer << std::endl;

	G_MapUserIndexDeviceInstanceId[G_CurrentUserIndex] = std::wstring(buffer);

	const auto handle = real_CreateFileW(
		lpFileName,
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile
	);

	G_CurrentUserIndex++;

	return handle;
}

static void RunLookupForUserIndex()
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

	XINPUT_CAPABILITIES caps = { };

	//
	// Poor man's state reset
	// 
	G_MapUserIndexDeviceInstanceId[0] = L"<not connected>";
	G_MapUserIndexDeviceInstanceId[1] = L"<not connected>";
	G_MapUserIndexDeviceInstanceId[2] = L"<not connected>";
	G_MapUserIndexDeviceInstanceId[3] = L"<not connected>";

	//
	// We need to reset this, see comment below why
	// 
	G_CurrentUserIndex = 0;

	//
	// This call will implicitly provoke the CreateFile call we hooked earlier within the logic of XInputX_X.dll
	// 
	const auto ret = pGetCapabilities(
		/* no matter how many devices are connected, always pass in 0
		 * because if >1 devices are present, the first call to
		 * XInputGetCapabilities will always enumerate all detected
		 * controllers on the first call, no matter what user index
		 * you supply, so we need to keep track of the association
		 * ourselves when the detoured function gets called.
		 */
		0,
		/* this is the only supported flag and always remains the same */
		XINPUT_FLAG_GAMEPAD,
		/* we do not really need the result but oh well */
		&caps
	);

	//
	// Remove hooks
	// 
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach((void**)&real_CreateFileW, DetourCreateFileW);
	DetourTransactionCommit();
}

int main()
{
	RunLookupForUserIndex();

	for (const auto& kv : G_MapUserIndexDeviceInstanceId)
	{
		std::wcout << L"Player Index: " << kv.first << " has Instance ID " << kv.second << std::endl;
	}

	getchar();
}
