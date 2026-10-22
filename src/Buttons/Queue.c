/*++

Module Name:

  Queue.c

Abstract:

  Control queue and IOCTL handlers for TimelmButtons.sys.

  Supported control codes:

    IOCTL_TIMELM_BUTTONS_GET_STATE    -> TIMELM_BUTTON_STATE
    IOCTL_TIMELM_BUTTONS_GET_MAPPING  -> TIMELM_BUTTON_MAPPING
    IOCTL_TIMELM_BUTTONS_SET_MAPPING  <- TIMELM_BUTTON_MAPPING
    IOCTL_TIMELM_BUTTONS_RAW_READ     <- TIMELM_BUTTON_RAW_REQUEST
                                         -> TIMELM_BUTTON_RAW_REQUEST + data
    IOCTL_TIMELM_BUTTONS_DUMP         <- TIMELM_BUTTON_RAW_REQUEST
                                         -> TIMELM_BUTTON_RAW_REQUEST + data
    IOCTL_TIMELM_BUTTONS_WAIT         <- TIMELM_BUTTON_WAIT_REQUEST
                                         -> TIMELM_BUTTON_STATE

--*/

#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, ButtonsQueueInitialize)
#endif

NTSTATUS
ButtonsQueueInitialize(
    _In_ WDFDEVICE Device
    )
{
    WDF_OBJECT_ATTRIBUTES queueAttributes;
    PQUEUE_CONTEXT queueContext;
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;

    PAGED_CODE();

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchParallel
    );

    queueConfig.EvtIoDeviceControl = ButtonsEvtIoDeviceControl;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueAttributes, QUEUE_CONTEXT);

    status = WdfIoQueueCreate(
                Device,
                &queueConfig,
                &queueAttributes,
                &queue
            );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    queueContext = QueueGetContext(queue);
    queueContext->Device = Device;

    return status;
}

VOID
ButtonsEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    PQUEUE_CONTEXT queueContext;
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status = STATUS_SUCCESS;

    queueContext = QueueGetContext(Queue);
    deviceContext = DeviceGetContext(queueContext->Device);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_QUEUE,
                "%!FUNC! ioctl 0x%08x in %u out %u",
                (unsigned int)IoControlCode,
                (unsigned int)InputBufferLength,
                (unsigned int)OutputBufferLength);

    switch (IoControlCode) {

    case IOCTL_TIMELM_BUTTONS_GET_STATE: {
        PVOID buffer = NULL;

        if (OutputBufferLength < sizeof(TIMELM_BUTTON_STATE)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveOutputBuffer(Request,
                                                sizeof(TIMELM_BUTTON_STATE),
                                                &buffer,
                                                NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        KeAcquireSpinLock(&deviceContext->Lock);
        RtlCopyMemory(buffer, &deviceContext->State, sizeof(TIMELM_BUTTON_STATE));
        KeReleaseSpinLock(&deviceContext->Lock);

        WdfRequestCompleteWithInformation(Request,
                                           STATUS_SUCCESS,
                                           (ULONG)sizeof(TIMELM_BUTTON_STATE));
        return;
    }

    case IOCTL_TIMELM_BUTTONS_GET_MAPPING: {
        PVOID buffer = NULL;

        if (OutputBufferLength < sizeof(TIMELM_BUTTON_MAPPING)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveOutputBuffer(Request,
                                                sizeof(TIMELM_BUTTON_MAPPING),
                                                &buffer,
                                                NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        KeAcquireSpinLock(&deviceContext->Lock);
        RtlCopyMemory(buffer, &deviceContext->Mapping, sizeof(TIMELM_BUTTON_MAPPING));
        KeReleaseSpinLock(&deviceContext->Lock);

        WdfRequestCompleteWithInformation(Request,
                                           STATUS_SUCCESS,
                                           (ULONG)sizeof(TIMELM_BUTTON_MAPPING));
        return;
    }

    case IOCTL_TIMELM_BUTTONS_SET_MAPPING: {
        TIMELM_BUTTON_MAPPING mapping;
        PVOID buffer = NULL;

        if (InputBufferLength < sizeof(TIMELM_BUTTON_MAPPING)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveInputBuffer(Request,
                                               sizeof(TIMELM_BUTTON_MAPPING),
                                               &buffer,
                                               NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        RtlCopyMemory(&mapping, buffer, sizeof(mapping));

        if (mapping.Version != TIMELM_BUTTON_MAPPING_VERSION ||
            mapping.Count != TIMELM_BUTTON_COUNT) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
                        "Invalid mapping version %u count %u",
                        mapping.Version,
                        mapping.Count);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        KeAcquireSpinLock(&deviceContext->Lock);
        deviceContext->Mapping = mapping;
        RtlZeroMemory(deviceContext->Entries, sizeof(deviceContext->Entries));
        deviceContext->State.Buttons = 0;
        KeReleaseSpinLock(&deviceContext->Lock);

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "Mapping updated");

        WdfRequestComplete(Request, STATUS_SUCCESS);
        return;
    }

    case IOCTL_TIMELM_BUTTONS_RAW_READ:
    case IOCTL_TIMELM_BUTTONS_DUMP: {
        TIMELM_BUTTON_RAW_REQUEST rawRequest;
        UINT8 data[TIMELM_BUTTONS_MAX_TRANSFER];
        PVOID buffer = NULL;

        if (InputBufferLength < sizeof(TIMELM_BUTTON_RAW_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveInputBuffer(Request,
                                               sizeof(TIMELM_BUTTON_RAW_REQUEST),
                                               &buffer,
                                               NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        RtlCopyMemory(&rawRequest, buffer, sizeof(rawRequest));

        if (rawRequest.Length == 0 || rawRequest.Length > TIMELM_BUTTONS_MAX_TRANSFER) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (OutputBufferLength < sizeof(TIMELM_BUTTON_RAW_REQUEST) + rawRequest.Length) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = ButtonsReadPmic(deviceContext,
                                 rawRequest.Sid,
                                 rawRequest.Addr,
                                 data,
                                 rawRequest.Length);
        if (!NT_SUCCESS(status)) {
            break;
        }

        status = WdfRequestRetrieveOutputBuffer(Request,
                                                sizeof(TIMELM_BUTTON_RAW_REQUEST) + rawRequest.Length,
                                                &buffer,
                                                NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        RtlCopyMemory(buffer, &rawRequest, sizeof(rawRequest));
        RtlCopyMemory((PUINT8)buffer + sizeof(TIMELM_BUTTON_RAW_REQUEST),
                      data,
                      rawRequest.Length);

        WdfRequestCompleteWithInformation(Request,
                                           STATUS_SUCCESS,
                                           (ULONG)(sizeof(TIMELM_BUTTON_RAW_REQUEST) + rawRequest.Length));
        return;
    }

    case IOCTL_TIMELM_BUTTONS_WAIT: {
        TIMELM_BUTTON_WAIT_REQUEST waitRequest;
        PVOID buffer = NULL;
        LONG timeout = 1000;

        if (InputBufferLength < sizeof(TIMELM_BUTTON_WAIT_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveInputBuffer(Request,
                                               sizeof(TIMELM_BUTTON_WAIT_REQUEST),
                                               &buffer,
                                               NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        RtlCopyMemory(&waitRequest, buffer, sizeof(waitRequest));

        if (waitRequest.TimeoutMillis > 5000) {
            timeout = 5000;
        } else {
            timeout = (LONG)waitRequest.TimeoutMillis;
        }

        if (timeout > 0) {
            NTSTATUS waitStatus;
            LARGE_INTEGER timeoutValue;

            //
            // The timeout is relative and expressed in 100 ns units.
            //
            timeoutValue.QuadPart = -((LONGLONG)timeout * 10000);

            waitStatus = KeWaitForSingleObject(&deviceContext->ChangeEvent,
                                               ExecutiveWaitMode,
                                               KernelMode,
                                               FALSE,
                                               &timeoutValue);
            if (waitStatus == STATUS_TIMEOUT) {
                /* Not an error, the caller gets the current state. */
            } else if (!NT_SUCCESS(waitStatus)) {
                status = waitStatus;
                break;
            }
        }

        if (OutputBufferLength < sizeof(TIMELM_BUTTON_STATE)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveOutputBuffer(Request,
                                                sizeof(TIMELM_BUTTON_STATE),
                                                &buffer,
                                                NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        KeAcquireSpinLock(&deviceContext->Lock);
        RtlCopyMemory(buffer, &deviceContext->State, sizeof(TIMELM_BUTTON_STATE));
        KeReleaseSpinLock(&deviceContext->Lock);

        WdfRequestCompleteWithInformation(Request,
                                           STATUS_SUCCESS,
                                           (ULONG)sizeof(TIMELM_BUTTON_STATE));
        return;
    }

    default:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "Unknown ioctl 0x%08x", IoControlCode);
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestComplete(Request, status);
}
