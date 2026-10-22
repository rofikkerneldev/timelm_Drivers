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
        ULONG copyLength;

        if (OutputBufferLength < TIMELM_BUTTON_STATE_LEGACY_SIZE) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        //
        // A caller of the first version of the structure asks for 48 bytes
        // and gets the first 48 bytes of the current one, the rest is the
        // state of the HID face.
        //
        copyLength = (OutputBufferLength < sizeof(TIMELM_BUTTON_STATE))
                   ? (ULONG)OutputBufferLength : (ULONG)sizeof(TIMELM_BUTTON_STATE);

        status = WdfRequestRetrieveOutputBuffer(Request,
                                                copyLength,
                                                &buffer,
                                                NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        WdfSpinLockAcquire(deviceContext->Lock);
        RtlCopyMemory(buffer, &deviceContext->State, copyLength);
        WdfSpinLockRelease(deviceContext->Lock);

        WdfRequestCompleteWithInformation(Request,
                                           STATUS_SUCCESS,
                                           copyLength);
        return;
    }

    case IOCTL_TIMELM_BUTTONS_GET_MAPPING: {
        PVOID buffer = NULL;
        ULONG copyLength;

        if (OutputBufferLength < TIMELM_BUTTON_MAPPING_LEGACY_SIZE) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        copyLength = (OutputBufferLength < sizeof(TIMELM_BUTTON_MAPPING))
                   ? (ULONG)OutputBufferLength : (ULONG)sizeof(TIMELM_BUTTON_MAPPING);

        status = WdfRequestRetrieveOutputBuffer(Request,
                                                copyLength,
                                                &buffer,
                                                NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        WdfSpinLockAcquire(deviceContext->Lock);
        RtlCopyMemory(buffer, &deviceContext->Mapping, copyLength);
        WdfSpinLockRelease(deviceContext->Lock);

        WdfRequestCompleteWithInformation(Request,
                                           STATUS_SUCCESS,
                                           copyLength);
        return;
    }

    case IOCTL_TIMELM_BUTTONS_SET_MAPPING: {
        TIMELM_BUTTON_MAPPING mapping;
        PVOID buffer = NULL;
        ULONG copyLength;

        if (InputBufferLength < TIMELM_BUTTON_MAPPING_LEGACY_SIZE) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        copyLength = (InputBufferLength < sizeof(TIMELM_BUTTON_MAPPING))
                   ? (ULONG)InputBufferLength : (ULONG)sizeof(TIMELM_BUTTON_MAPPING);

        status = WdfRequestRetrieveInputBuffer(Request,
                                               copyLength,
                                               &buffer,
                                               NULL);
        if (!NT_SUCCESS(status)) {
            break;
        }

        RtlZeroMemory(&mapping, sizeof(mapping));
        RtlCopyMemory(&mapping, buffer, copyLength);

        if (copyLength >= sizeof(TIMELM_BUTTON_MAPPING) &&
            mapping.Version == TIMELM_BUTTON_MAPPING_VERSION &&
            mapping.Count == TIMELM_BUTTON_COUNT) {
            //
            // The current version, the actions are used as they are.
            //
        } else {
            TIMELM_BUTTON_MAPPING fallback;

            RtlZeroMemory(&fallback, sizeof(fallback));
            ButtonsLoadDefaultMapping(&fallback);

            if ((mapping.Version != 1u &&
                 mapping.Version != TIMELM_BUTTON_MAPPING_VERSION) ||
                mapping.Count != TIMELM_BUTTON_COUNT) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
                            "Invalid mapping version %u count %u",
                            mapping.Version,
                            mapping.Count);
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            //
            // A tool that speaks the first version, or a caller that did
            // not send the actions at all.  The sources are taken over, the
            // actions come from the default mapping.
            //
            RtlCopyMemory(mapping.Action,
                          fallback.Action,
                          sizeof(mapping.Action));
            mapping.Version = TIMELM_BUTTON_MAPPING_VERSION;
        }

        WdfSpinLockAcquire(deviceContext->Lock);
        deviceContext->Mapping = mapping;
        RtlZeroMemory(deviceContext->Entries, sizeof(deviceContext->Entries));
        deviceContext->State.Buttons = 0;
        WdfSpinLockRelease(deviceContext->Lock);

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

            //
            // The wait reason is Executive and the processor mode is Kernel.
            // Both values are passed numerically so that the driver does not
            // depend on the spelling of the enum members.
            //
            waitStatus = KeWaitForSingleObject(&deviceContext->ChangeEvent,
                                               (KWAIT_REASON)0,
                                               (KPROCESSOR_MODE)0,
                                               FALSE,
                                               &timeoutValue);
            if (waitStatus == STATUS_TIMEOUT) {
                /* Not an error, the caller gets the current state. */
            } else if (!NT_SUCCESS(waitStatus)) {
                status = waitStatus;
                break;
            }
        }

        if (OutputBufferLength < TIMELM_BUTTON_STATE_LEGACY_SIZE) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        {
            ULONG copyLength;

            copyLength = (OutputBufferLength < sizeof(TIMELM_BUTTON_STATE))
                       ? (ULONG)OutputBufferLength : (ULONG)sizeof(TIMELM_BUTTON_STATE);

            status = WdfRequestRetrieveOutputBuffer(Request,
                                                    copyLength,
                                                    &buffer,
                                                    NULL);
            if (!NT_SUCCESS(status)) {
                break;
            }

            WdfSpinLockAcquire(deviceContext->Lock);
            RtlCopyMemory(buffer, &deviceContext->State, copyLength);
            WdfSpinLockRelease(deviceContext->Lock);

            WdfRequestCompleteWithInformation(Request,
                                               STATUS_SUCCESS,
                                               copyLength);
            return;
        }
    }

    default:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "Unknown ioctl 0x%08x", IoControlCode);
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestComplete(Request, status);
}
