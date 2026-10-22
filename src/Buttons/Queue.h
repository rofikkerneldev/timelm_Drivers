/*++

Module Name:

  Queue.h

Abstract:

  Queue declarations for TimelmButtons.sys.

--*/

#pragma once

EXTERN_C_START

typedef struct _QUEUE_CONTEXT {
    WDFDEVICE Device;
} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, QueueGetContext)

NTSTATUS
ButtonsQueueInitialize(
    _In_ WDFDEVICE Device
    );

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL ButtonsEvtIoDeviceControl;

EXTERN_C_END
