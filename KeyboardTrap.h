
#include <ntddk.h>
#include <wdf.h>

#include <kbdmou.h>
#include <ntddmou.h>

#include <ntddkbd.h>
#include <ntdd8042.h>


//======================================================================================================================

#if DBG
	#define TRAP() DbgBreakPoint()
	#define DebugPrint(_x_) DbgPrint _x_
#else   // DBG
	#define TRAP()
	#define DebugPrint(_x_)
#endif

//======================================================================================================================

DEFINE_GUID(GUID_DEVINTERFACE_KEYBOARDTRAP, 0xea708fa1, 0x5397, 0x4eec, 0x8d, 0x13, 0xf4, 0x97, 0x3b, 0x69, 0x29, 0x32);

//======================================================================================================================

typedef struct _DEVICE_CONTEXT {
	CONNECT_DATA UpperConnectData;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;


WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//======================================================================================================================

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD KeyboardTrapEvtDeviceAdd;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL KeyboardTrapEvtIoInternalDeviceControl;

void KeyboardTrapServiceCallback(DEVICE_OBJECT* deviceObject, KEYBOARD_INPUT_DATA* inputDataStart, KEYBOARD_INPUT_DATA* inputDataEnd, ULONG* inputDataConsumed);

//======================================================================================================================

typedef struct _WORKER_ITEM_CONTEXT {
	BOOLEAN hasRun;
	char buffer[2];

} WORKER_ITEM_CONTEXT, * PWORKER_ITEM_CONTEXT;

// definitions
NTSTATUS openFile();

NTSTATUS
CreateWorkItemWrite(
	WDFDEVICE DeviceObject
);

NTSTATUS WriteToFile(char buffer[2]);

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
	WORKER_ITEM_CONTEXT, GetWorkItemContext);



typedef struct _DEVICE_EXTENSION
{
	WDFDEVICE WdfDevice;

	//
	// Number of creates sent down
	//
	//LONG EnableCount;

	//
	// The real connect data that this driver reports to
	//
	CONNECT_DATA UpperConnectData;

	//
	// Previous initialization and hook routines (and context)
	//
	PVOID									UpperContext;
	PI8042_KEYBOARD_INITIALIZATION_ROUTINE	UpperInitializationRoutine;
	PI8042_KEYBOARD_ISR						UpperIsrHook;

	//
	// Context for IsrWritePort, QueueKeyboardPacket
	//
	IN PVOID CallContext;

	//
	// Worker item
	//
	WDFWORKITEM workItem;

} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
	DEVICE_EXTENSION, GetDeviceExtension);