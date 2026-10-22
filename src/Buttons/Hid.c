/*++

Module Name:

  Hid.c

Abstract:

  The HID face of TimelmButtons.sys.

  The driver can report the buttons to Windows as a real HID device, so
  that the volume keys change the volume of the system and the power key
  opens the power menu of Windows.  The device is created with the Virtual
  HID Framework (VHF), the same mechanism that the driver of the topaz
  project uses for the buttons of the Redmi Note 12.

  The report descriptor carries two reports:

      report 1   generic desktop / system control, system power down
      report 2   consumer control, volume increment and volume decrement

  The HID face is optional.  It is switched on per button with the mapping
  (Action[].HidEnabled) and it is only used for a short press, a long press
  belongs to the user mode agent.  If the VHF device cannot be created the
  driver keeps running, the agent path stays usable.

--*/

#include "driver.h"
#include "hid.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, ButtonsHidInitialize)
#pragma alloc_text (PAGE, ButtonsHidStart)
#endif

//
// Vendor and product id of the virtual device.  The vendor id of Qualcomm
// is used, the product id is the model of the phone.
//
#define TIMELM_HID_VENDOR_ID   0x05C6
#define TIMELM_HID_PRODUCT_ID  0x6125

//
// Report id 1: system power down.  Report id 2: volume increment /
// decrement, bit 0 is the increment, bit 1 is the decrement.
//
#define TIMELM_HID_REPORT_POWER  1
#define TIMELM_HID_REPORT_VOLUME 2

static UCHAR g_TimelmHidReportDescriptor[] = {
    0x05, 0x01,        /* Usage Page (Generic Desktop)                  */
    0x09, 0x80,        /* Usage (System Control)                        */
    0xA1, 0x01,        /* Collection (Application)                      */
    0x85, 0x01,        /*   Report ID 1                                 */
    0x15, 0x00,        /*   Logical Minimum (0)                         */
    0x25, 0x01,        /*   Logical Maximum (1)                         */
    0x75, 0x01,        /*   Report Size (1)                             */
    0x95, 0x01,        /*   Report Count (1)                            */
    0x09, 0x81,        /*   Usage (System Power Down)                   */
    0x81, 0x02,        /*   Input (Data, Variable, Absolute)            */
    0x95, 0x07,        /*   Report Count (7)                            */
    0x81, 0x03,        /*   Input (Constant)                            */
    0xC0,              /* End Collection                                */
    0x05, 0x0C,        /* Usage Page (Consumer)                         */
    0x09, 0x01,        /* Usage (Consumer Control)                      */
    0xA1, 0x01,        /* Collection (Application)                      */
    0x85, 0x02,        /*   Report ID 2                                 */
    0x15, 0x00,        /*   Logical Minimum (0)                         */
    0x25, 0x01,        /*   Logical Maximum (1)                         */
    0x75, 0x01,        /*   Report Size (1)                             */
    0x95, 0x02,        /*   Report Count (2)                            */
    0x09, 0xE9,        /*   Usage (Volume Increment)                    */
    0x09, 0xEA,        /*   Usage (Volume Decrement)                    */
    0x81, 0x02,        /*   Input (Data, Variable, Absolute)            */
    0x95, 0x06,        /*   Report Count (6)                            */
    0x81, 0x03,        /*   Input (Constant)                            */
    0xC0               /* End Collection                                */
};

NTSTATUS
ButtonsHidInitialize(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    VHF_CONFIG config;
    NTSTATUS status;

    PAGED_CODE();

    if (Context->HidHandle != NULL) {
        return STATUS_SUCCESS;
    }

    VHF_CONFIG_INIT(&config,
                    WdfDeviceWdmGetDeviceObject(Context->Device),
                    (USHORT)sizeof(g_TimelmHidReportDescriptor),
                    (PUCHAR)g_TimelmHidReportDescriptor);

    config.VhfClientContext = Context;
    config.VendorID = (USHORT)TIMELM_HID_VENDOR_ID;
    config.ProductID = (USHORT)TIMELM_HID_PRODUCT_ID;

    status = VhfCreate(&config, &Context->HidHandle);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "VhfCreate returned %!STATUS!",
                status);

    if (!NT_SUCCESS(status)) {
        Context->HidHandle = NULL;
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
ButtonsHidStart(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    NTSTATUS status;

    PAGED_CODE();

    if (Context->HidHandle == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    status = VhfStart(Context->HidHandle);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "VhfStart returned %!STATUS!",
                status);

    if (NT_SUCCESS(status)) {
        Context->HidStarted = TRUE;
    }

    return status;
}

VOID
ButtonsHidReport(
    _Inout_ PDEVICE_CONTEXT Context,
    _In_ UINT32 HidAction
    )
{
    UCHAR buffer[2];
    HID_XFER_PACKET packet;
    VHFHANDLE handle;

    //
    // The handle is protected by the spin lock.  While the device is
    // removed no new report is started and the delete waits for the
    // running one, so that the handle is never used after it was deleted.
    //
    WdfSpinLockAcquire(Context->Lock);
    if (Context->HidStopping || Context->HidHandle == NULL || !Context->HidStarted) {
        WdfSpinLockRelease(Context->Lock);
        return;
    }

    handle = Context->HidHandle;
    Context->HidBusy++;
    KeClearEvent(&Context->HidIdleEvent);
    WdfSpinLockRelease(Context->Lock);

    RtlZeroMemory(buffer, sizeof(buffer));

    switch (HidAction) {
    case TIMELM_HID_VOLUME_INC:
        buffer[0] = TIMELM_HID_REPORT_VOLUME;
        buffer[1] = 0x01;
        break;

    case TIMELM_HID_VOLUME_DEC:
        buffer[0] = TIMELM_HID_REPORT_VOLUME;
        buffer[1] = 0x02;
        break;

    case TIMELM_HID_POWER_DOWN:
        buffer[0] = TIMELM_HID_REPORT_POWER;
        buffer[1] = 0x01;
        break;

    default:
        break;
    }

    if (buffer[0] != 0) {
        RtlZeroMemory(&packet, sizeof(packet));
        packet.reportBuffer = buffer;
        packet.reportBufferLen = (ULONG)sizeof(buffer);
        packet.reportId = buffer[0];

        //
        // The usage is momentary, the report with the bit set is the press
        // and the report with the bit cleared is the release.  Windows needs
        // both, otherwise the key stays pressed.
        //
        (void)VhfReadReportSubmit(handle, &packet);

        buffer[1] = 0x00;
        (void)VhfReadReportSubmit(handle, &packet);

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "HID report %u action %u sent",
                    (unsigned int)packet.reportId,
                    (unsigned int)HidAction);
    }

    WdfSpinLockAcquire(Context->Lock);
    Context->HidBusy--;
    if (Context->HidBusy == 0) {
        KeSetEvent(&Context->HidIdleEvent, IO_NO_INCREMENT, FALSE);
    }
    WdfSpinLockRelease(Context->Lock);
}

VOID
ButtonsHidStop(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    LARGE_INTEGER timeout;
    VHFHANDLE handle;
    LONG busy;

    //
    // No new report is started any more, a running one is waited for.
    //
    WdfSpinLockAcquire(Context->Lock);
    Context->HidStopping = TRUE;
    busy = Context->HidBusy;
    WdfSpinLockRelease(Context->Lock);

    if (busy > 0) {
        timeout.QuadPart = -10000000LL;
        (void)KeWaitForSingleObject(&Context->HidIdleEvent,
                                    Executive,
                                    KernelMode,
                                    FALSE,
                                    &timeout);
    }

    WdfSpinLockAcquire(Context->Lock);
    handle = Context->HidHandle;
    Context->HidHandle = NULL;
    Context->HidStarted = FALSE;
    Context->State.HidReady = 0;
    WdfSpinLockRelease(Context->Lock);

    if (handle != NULL) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "the virtual HID device is deleted");
        VhfDelete(handle, TRUE);
    }
}
