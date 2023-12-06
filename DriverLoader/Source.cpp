#include <filesystem>
#include <string>
#include <windows.h>
#include <iostream>
#include <string.h>
#include <tchar.h>

#define IOCTL_CUSTOM_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _INPUT_BUFFER {
	ULONG NumericValue;
} INPUT_BUFFER, * PINPUT_BUFFER;

BOOL StopAndRemoveService(const TCHAR* serviceName) {
	SC_HANDLE hSCManager, hService;
	SERVICE_STATUS serviceStatus;

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCManager == NULL) {
		_tprintf(_T("OpenSCManager failed (%d)\n"), GetLastError());
		return FALSE;
	}

	hService = OpenService(hSCManager, serviceName, SERVICE_ALL_ACCESS);
	if (hService == NULL) {
		_tprintf(_T("OpenService failed (%d)\n"), GetLastError());
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	if (!ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus)) {
		_tprintf(_T("ControlService failed (%d)\n"), GetLastError());
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	while (QueryServiceStatus(hService, &serviceStatus)) {
		if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
			Sleep(1000);
		}
		else {
			break;
		}
	}

	if (!DeleteService(hService)) {
		_tprintf(_T("DeleteService failed (%d)\n"), GetLastError());
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
	return TRUE;
}

int main(int argc, _TCHAR* argv[]) {
	SC_HANDLE schSCManager, schService;
	BOOL bRet;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL) {
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return 1;
	}

	std::filesystem::path currentPath = std::filesystem::current_path();
	std::wstring currentDriverPath = currentPath.wstring() + L"\\Driver.sys";
	LPCWSTR currentDriverPathLPCWSTR = currentDriverPath.c_str();

	schService = CreateService(
		schSCManager,
		_T("KernelKillProcessService"),
		_T("Kernel Kill Process Service"),
		SERVICE_ALL_ACCESS,
		SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		currentDriverPathLPCWSTR,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	);

	if (schService == NULL) {
		printf("CreateService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		CloseServiceHandle(schService);
		return 1;
	}

	bRet = StartService(schService, 0, NULL);
	if (!bRet) {
		printf("StartServie failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		CloseServiceHandle(schService);
	}
	else {
		printf("Service started successfully.\n");
	}

	HANDLE hDevice = CreateFile(
		_T("\\\\.\\KernelKillProcesModule"),
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("failed to open %d\n", GetLastError());
	}

	INPUT_BUFFER inputBuffer;
	scanf_s("%d", &inputBuffer.NumericValue);

	DWORD bytesReturned;
	if (DeviceIoControl(hDevice, IOCTL_CUSTOM_CODE, &inputBuffer, sizeof(inputBuffer), NULL, 0, &bytesReturned, NULL)) {
		printf("Successful\n");
	}
	else {
		printf("Failed\n");
	}

	CloseHandle(hDevice);
	StopAndRemoveService(_T("KernelKillProcessService"));
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return 0;
}