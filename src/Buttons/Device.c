/*++

Module Name:

  Device.c

Abstract:

  Device object, PMIC (SPMI) access and the button sampler for
  TimelmButtons.sys.

  All button state is read from the PM8150 primary PMIC (SPMI slave 0)
  through the GUID_SPMI_INTERFACE that is published by SPMI.sys:

      Power       -> 0x0810 bit0   (PON RT_STS, KPDPWR_N)  pressed = set
      Volume Down -> 0x0810 bit1   (PON RT_STS, RESIN_N)   pressed = set
      Volume Up   -> 0xC510 bit0   (PMIC GPIO6 MPP_RT_STS) pressed = clear

  The mapping can be replaced at run time with IOCTL
  IOCTL_TIMELM_BUTTONS_SET_MAPPING, which is used by the discovery tooling
  to brute force the real register layout.

--*/

#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, ButtonsCreateDevice)
#endif

#define TIMELM_SPMI_RETRY_TICKS 25   /* ~500 ms with a 20 ms poll period */

static ULONGLONG
ButtonsSystemMs(
    VOID
    );

NTSTATUS
ButtonsCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_OBJECT_ATTRIBUTES ioTargetAttributes;
    WDF_OBJECT_ATTRIBUTES lockAttributes;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;
    WDF_WORKITEM_CONFIG workItemConfig;
    WDF_OBJECT_ATTRIBUTES workItemAttributes;
    NTSTATUS status;

    PAGED_CODE();

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceCreate failed %!STATUS!", status);
        return status;
    }

    deviceContext = DeviceGetContext(device);
    RtlZeroMemory(deviceContext, sizeof(DEVICE_CONTEXT));

    deviceContext->Device = device;

    WDF_OBJECT_ATTRIBUTES_INIT(&lockAttributes);
    lockAttributes.ParentObject = device;

    status = WdfSpinLockCreate(&lockAttributes, &deviceContext->Lock);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfSpinLockCreate failed %!STATUS!", status);
        return status;
    }

    KeInitializeEvent(&deviceContext->ChangeEvent, NotificationEvent, FALSE);

    ButtonsLoadDefaultMapping(&deviceContext->Mapping);

    deviceContext->State.Version = TIMELM_BUTTON_STATE_VERSION;
    deviceContext->State.Ready = 0;
    deviceContext->State.Buttons = 0;
    deviceContext->State.ChangeCounter = 0;

    //
    // The symbolic link is the user mode entry point: \\.\TimelmButtons
    //
    {
        DECLARE_CONST_UNICODE_STRING(dosDeviceName, TIMELM_BUTTONS_SYMBOLIC_LINK);

        status = WdfDeviceCreateSymbolicLink(device, &dosDeviceName);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "WdfDeviceCreateSymbolicLink failed %!STATUS!",
                        status);
        }
    }

    //
    // One io target is created for the whole lifetime of the device, it is
    // opened and closed again whenever the SPMI interface is (re)acquired.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&ioTargetAttributes);
    ioTargetAttributes.ParentObject = device;

    status = WdfIoTargetCreate(device, &ioTargetAttributes, &deviceContext->IoTarget);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfIoTargetCreate failed %!STATUS!", status);
        return status;
    }

    status = ButtonsQueueInitialize(device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "ButtonsQueueInitialize failed %!STATUS!", status);
        return status;
    }

    deviceContext->StartMs = ButtonsSystemMs();

    //
    // Try to reach the PMIC right away. The SPMI driver is a boot start
    // driver, but it may still be starting when we are loaded, in that case
    // the sampler retries.
    //
    status = ButtonsAcquireSpmi(device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                    "SPMI interface is not available yet (%!STATUS!), will retry",
                    status);
    }

    //
    // The timer runs as a DPC at DISPATCH_LEVEL, it only re-arms itself and
    // hands the sampling over to a work item, so that the PMIC is always
    // accessed at PASSIVE_LEVEL.  The automatic serialization is disabled for
    // both objects, otherwise a blocked WAIT ioctl would stop the sampler.
    //
    WDF_TIMER_CONFIG_INIT(&timerConfig, ButtonsEvtTimerFired);
    timerConfig.AutomaticSerialization = FALSE;

    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = device;

    status = WdfTimerCreate(&timerConfig,
                            &timerAttributes,
                            &deviceContext->Timer);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfTimerCreate failed %!STATUS!", status);
        return status;
    }

    WDF_WORKITEM_CONFIG_INIT(&workItemConfig, ButtonsEvtWorkItem);
    workItemConfig.AutomaticSerialization = FALSE;

    WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
    workItemAttributes.ParentObject = device;

    status = WdfWorkItemCreate(&workItemConfig,
                               &workItemAttributes,
                               &deviceContext->WorkItem);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfWorkItemCreate failed %!STATUS!", status);
        return status;
    }

    WdfTimerStart(deviceContext->Timer,
                  TIMELM_BUTTONS_REL_TIMEOUT_MS(TIMELM_BUTTONS_POLL_PERIOD_MS));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "%!FUNC! TimelmButtons started, poll %d ms",
                TIMELM_BUTTONS_POLL_PERIOD_MS);

    return STATUS_SUCCESS;
}

NTSTATUS
ButtonsAcquireSpmi(
    _In_ WDFDEVICE Device
    )
{
    PDEVICE_CONTEXT deviceContext;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    NTSTATUS status;

    deviceContext = DeviceGetContext(Device);

    if (deviceContext->SpmiReady) {
        return STATUS_SUCCESS;
    }

    if (deviceContext->IoTarget == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    {
        DECLARE_CONST_UNICODE_STRING(spmiDosDeviceName, SPMI_SYMBOLIC_NAME_STRING);

        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
            &openParams,
            &spmiDosDeviceName,
            (GENERIC_READ | GENERIC_WRITE));

        openParams.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
        openParams.CreateDisposition = FILE_OPEN;
        openParams.FileAttributes = FILE_ATTRIBUTE_NORMAL;

        status = WdfIoTargetOpen(deviceContext->IoTarget, &openParams);
    }

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "WdfIoTargetOpen failed %!STATUS!", status);
        return status;
    }

    RtlZeroMemory(&deviceContext->SPMIInterface, sizeof(SPMI_INTERFACE));

    status = WdfIoTargetQueryForInterface(deviceContext->IoTarget,
        &GUID_SPMI_INTERFACE,
        (PINTERFACE)&deviceContext->SPMIInterface,
        sizeof(SPMI_INTERFACE),
        1,
        NULL);

    WdfIoTargetClose(deviceContext->IoTarget);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                    "WdfIoTargetQueryForInterface failed %!STATUS!",
                    status);
        RtlZeroMemory(&deviceContext->SPMIInterface, sizeof(SPMI_INTERFACE));
        return status;
    }

    deviceContext->SpmiReady = TRUE;
    deviceContext->RetryCounter = 0;

    //
    // Identify the PMIC. Page 0 register 4 holds the peripheral type and
    // register 5 the subtype, they are only used for diagnostics.
    //
    {
        UINT8 pmicInfo[2] = { 0, 0 };

        if (NT_SUCCESS(ButtonsReadPmic(deviceContext, TIMELM_PMIC_SID_PRIMARY, 0x0004, pmicInfo, 2))) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                        "PMIC sid %d type 0x%02x subtype 0x%02x",
                        (int)TIMELM_PMIC_SID_PRIMARY,
                        (unsigned int)pmicInfo[0],
                        (unsigned int)pmicInfo[1]);
        }
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "SPMI interface acquired");

    return STATUS_SUCCESS;
}

VOID
ButtonsLoadDefaultMapping(
    _Out_ PTIMELM_BUTTON_MAPPING Mapping
    )
{
    RtlZeroMemory(Mapping, sizeof(*Mapping));

    Mapping->Version = TIMELM_BUTTON_MAPPING_VERSION;
    Mapping->Count = TIMELM_BUTTON_COUNT;

    //
    // Power key, PON KPDPWR_N.
    //
    Mapping->Source[TIMELM_BUTTON_POWER].Sid = TIMELM_PMIC_SID_PRIMARY;
    Mapping->Source[TIMELM_BUTTON_POWER].Addr = TIMELM_PON_RT_STS;
    Mapping->Source[TIMELM_BUTTON_POWER].Mask = TIMELM_PON_KPDPWR_N_MASK;
    Mapping->Source[TIMELM_BUTTON_POWER].Polarity = TIMELM_POLARITY_HIGH;
    Mapping->Source[TIMELM_BUTTON_POWER].Enabled = 1;

    //
    // Volume down key, PON RESIN_N.
    //
    Mapping->Source[TIMELM_BUTTON_VOLUME_DOWN].Sid = TIMELM_PMIC_SID_PRIMARY;
    Mapping->Source[TIMELM_BUTTON_VOLUME_DOWN].Addr = TIMELM_PON_RT_STS;
    Mapping->Source[TIMELM_BUTTON_VOLUME_DOWN].Mask = TIMELM_PON_RESIN_N_MASK;
    Mapping->Source[TIMELM_BUTTON_VOLUME_DOWN].Polarity = TIMELM_POLARITY_HIGH;
    Mapping->Source[TIMELM_BUTTON_VOLUME_DOWN].Enabled = 1;

    //
    // Volume up key, PMIC GPIO6, active low.
    //
    Mapping->Source[TIMELM_BUTTON_VOLUME_UP].Sid = TIMELM_PMIC_SID_PRIMARY;
    Mapping->Source[TIMELM_BUTTON_VOLUME_UP].Addr = TIMELM_PMIC_GPIO_VOL_UP_ADDR;
    Mapping->Source[TIMELM_BUTTON_VOLUME_UP].Mask = 0x01;
    Mapping->Source[TIMELM_BUTTON_VOLUME_UP].Polarity = TIMELM_POLARITY_LOW;
    Mapping->Source[TIMELM_BUTTON_VOLUME_UP].Enabled = 1;
}

NTSTATUS
ButtonsReadPmic(
    _In_ PDEVICE_CONTEXT Context,
    _In_ UINT32 Sid,
    _In_ UINT32 Addr,
    _Out_writes_bytes_opt_(Length) PUINT8 Data,
    _In_ UINT32 Length
    )
{
    if (Length == 0 || Length > TIMELM_BUTTONS_MAX_TRANSFER) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Sid >= 16) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!Context->SpmiReady) {
        return STATUS_DEVICE_NOT_READY;
    }

    return Context->SPMIInterface.Read(Context->SPMIInterface.Interface.Context,
                                       (UINT8)Sid,
                                       (UINT16)Addr,
                                       Data,
                                       (UINT8)Length);
}

//
// Milliseconds since boot, used as the time base of the driver.
//
static ULONGLONG
ButtonsSystemMs(
    VOID
    )
{
    LARGE_INTEGER systemTime;

    KeQuerySystemTime(&systemTime);

    return (ULONGLONG)systemTime.QuadPart / 10000ULL;
}

ULONG64
ButtonsNowMs(
    _In_ PDEVICE_CONTEXT Context
    )
{
    ULONGLONG now;

    now = ButtonsSystemMs();

    if (Context->StartMs == 0 || now < Context->StartMs) {
        return 0;
    }

    return now - Context->StartMs;
}

VOID
ButtonsEvtTimerFired(
    _In_ WDFTIMER Timer
    )
{
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;

    device = (WDFDEVICE)WdfTimerGetParentObject(Timer);
    deviceContext = DeviceGetContext(device);

    //
    // Re-arm the one shot timer for the next sample and let the work item do
    // the PMIC access at PASSIVE_LEVEL.
    //
    WdfTimerStart(Timer, TIMELM_BUTTONS_REL_TIMEOUT_MS(TIMELM_BUTTONS_POLL_PERIOD_MS));

    WdfWorkItemEnqueue(deviceContext->WorkItem);
}

VOID
ButtonsEvtWorkItem(
    _In_ WDFWORKITEM WorkItem
    )
{
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;

    device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
    deviceContext = DeviceGetContext(device);

    ButtonsSample(deviceContext);
}

VOID
ButtonsSample(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    TIMELM_BUTTON_MAPPING mapping;
    BOOLEAN pressedNow[TIMELM_BUTTON_COUNT];
    BOOLEAN sampleValid[TIMELM_BUTTON_COUNT];
    UINT32 cacheSid[TIMELM_BUTTON_COUNT];
    UINT32 cacheAddr[TIMELM_BUTTON_COUNT];
    UINT8 cacheValue[TIMELM_BUTTON_COUNT];
    BOOLEAN cacheValid[TIMELM_BUTTON_COUNT];
    ULONG i;
    ULONG j;
    ULONG now;
    BOOLEAN changed = FALSE;

    if (!Context->SpmiReady) {
        //
        // The SPMI driver may still be starting, retry every ~500 ms.
        // Acquiring the interface must happen at PASSIVE_LEVEL.
        //
        if (KeGetCurrentIrql() == PASSIVE_LEVEL) {
            Context->RetryCounter++;
            if (Context->RetryCounter >= TIMELM_SPMI_RETRY_TICKS) {
                Context->RetryCounter = 0;
                (void)ButtonsAcquireSpmi(Context->Device);
            }
        }
        return;
    }

    RtlZeroMemory(pressedNow, sizeof(pressedNow));
    RtlZeroMemory(sampleValid, sizeof(sampleValid));
    RtlZeroMemory(cacheValid, sizeof(cacheValid));

    WdfSpinLockAcquire(Context->Lock);
    mapping = Context->Mapping;
    WdfSpinLockRelease(Context->Lock);

    now = (ULONG)ButtonsNowMs(Context);

    for (i = 0; i < TIMELM_BUTTON_COUNT; i++) {
        UINT8 value = 0;
        BOOLEAN haveValue = FALSE;

        if (mapping.Source[i].Enabled == 0) {
            continue;
        }

        for (j = 0; j < TIMELM_BUTTON_COUNT; j++) {
            if (cacheValid[j] &&
                cacheSid[j] == mapping.Source[i].Sid &&
                cacheAddr[j] == mapping.Source[i].Addr) {
                value = cacheValue[j];
                haveValue = TRUE;
                break;
            }
        }

        if (!haveValue) {
            NTSTATUS status;

            status = ButtonsReadPmic(Context,
                                     mapping.Source[i].Sid,
                                     mapping.Source[i].Addr,
                                     &value,
                                     1);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                            "PMIC read sid %u addr 0x%x failed %!STATUS!",
                            mapping.Source[i].Sid,
                            mapping.Source[i].Addr,
                            status);
                continue;
            }

            for (j = 0; j < TIMELM_BUTTON_COUNT; j++) {
                if (!cacheValid[j]) {
                    cacheValid[j] = TRUE;
                    cacheSid[j] = mapping.Source[i].Sid;
                    cacheAddr[j] = mapping.Source[i].Addr;
                    cacheValue[j] = value;
                    break;
                }
            }
        }

        pressedNow[i] = ((value & (UINT8)mapping.Source[i].Mask) != 0) ? TRUE : FALSE;
        if (mapping.Source[i].Polarity == TIMELM_POLARITY_LOW) {
            pressedNow[i] = !pressedNow[i];
        }
        sampleValid[i] = TRUE;
    }

    WdfSpinLockAcquire(Context->Lock);

    for (i = 0; i < TIMELM_BUTTON_COUNT; i++) {
        PBUTTON_ENTRY entry;

        if (!sampleValid[i]) {
            continue;
        }

        entry = &Context->Entries[i];

        if (pressedNow[i] == entry->Candidate) {
            entry->CandidateCount++;
        } else {
            entry->Candidate = pressedNow[i];
            entry->CandidateCount = 1;
        }

        if (entry->CandidateCount >= TIMELM_BUTTONS_DEBOUNCE_SAMPLES &&
            entry->Stable != entry->Candidate) {

            entry->Stable = entry->Candidate;
            entry->LastEdgeMs = now;

            if (entry->Stable) {
                entry->PressCount++;
                entry->PressStartMs = now;
                entry->HeldMs = 0;
            } else {
                entry->HeldMs = now - entry->PressStartMs;
            }

            changed = TRUE;
        }

        if (entry->Stable) {
            Context->State.Buttons |= (1u << i);
            Context->State.HeldMillis[i] = (UINT32)(now - entry->PressStartMs);
        } else {
            Context->State.Buttons &= ~(1u << i);
            Context->State.HeldMillis[i] = (UINT32)entry->HeldMs;
        }

        Context->State.PressCount[i] = entry->PressCount;
    }

    if (changed) {
        Context->State.ChangeCounter++;
        Context->State.LastEdgeMillis = (UINT32)now;
    }

    Context->State.Ready = Context->SpmiReady ? 1 : 0;

    WdfSpinLockRelease(Context->Lock);

    if (changed) {
        KeSetEvent(&Context->ChangeEvent, IO_NO_INCREMENT, FALSE);
    }
}
