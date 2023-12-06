#include <ntddk.h>
#define IOCTL_CUSTOM_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _INPUT_BUFFER {
    ULONG NumericValue;
} INPUT_BUFFER, * PINPUT_BUFFER;

void ZwKillProcess(ULONG pid)
{
    HANDLE ProcessHandle = NULL;
    OBJECT_ATTRIBUTES obj;
    CLIENT_ID cid = { 0 };
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    InitializeObjectAttributes(&obj, NULL, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    cid.UniqueProcess = (HANDLE)pid;
    cid.UniqueThread = 0;
    ntStatus = ZwOpenProcess(&ProcessHandle, GENERIC_ALL, &obj, &cid);
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = ZwTerminateProcess(ProcessHandle, 0);
        if (NT_SUCCESS(ntStatus))
        {
            ZwClose(ProcessHandle);
        }
        else
        {
            ZwClose(ProcessHandle);
        }
    }
}

NTSTATUS MyDeviceControl(_In_ PDEVICE_OBJECT pDeviceObject, _Inout_ PIRP pIrp) {
    PIO_STACK_LOCATION irpStack;
    PVOID ioBuffer;
    ULONG inputBufferLength;
    PINPUT_BUFFER inputBuffer;

    UNREFERENCED_PARAMETER(pDeviceObject);

    irpStack = IoGetCurrentIrpStackLocation(pIrp);
    ioBuffer = pIrp->AssociatedIrp.SystemBuffer;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_CUSTOM_CODE:
        inputBuffer = (PINPUT_BUFFER)ioBuffer;
        inputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

        ZwKillProcess(inputBuffer->NumericValue);
        //DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "kill process successfully: %lu\n", inputBuffer->NumericValue);

        pIrp->IoStatus.Status = STATUS_SUCCESS;
        pIrp->IoStatus.Information = 0;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;

    default:
        pIrp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        pIrp->IoStatus.Information = 0;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }
}

NTSTATUS DriverRoutine(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

void DriverUnLoad(PDRIVER_OBJECT pDriverObj)
{
    UNICODE_STRING symbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\KernelKillProcesModule");
    IoDeleteSymbolicLink(&symbolicLinkName);
    IoDeleteDevice(pDriverObj->DeviceObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "驅動已卸載\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObj, PUNICODE_STRING pRegPath)
{
    UNREFERENCED_PARAMETER(pDriverObj);
    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\KernelKillProcesModule");
    PDEVICE_OBJECT deviceObject;
    NTSTATUS status = IoCreateDevice(
        pDriverObj,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to create device object: %08X\n", status));
        return status;
    }

    UNICODE_STRING symbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\KernelKillProcesModule");
    status = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);

    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to create symbolic link: %08X\n", status));
        IoDeleteDevice(deviceObject);
        return status;
    }

    pDriverObj->MajorFunction[IRP_MJ_CREATE] = DriverRoutine;
    pDriverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MyDeviceControl;
    pDriverObj->DriverUnload = DriverUnLoad;

    return STATUS_SUCCESS;
}
