/*++

Module Name:

  Driver.c

Abstract:

  Driver entry and device add callbacks for TimelmButtons.sys.

  TimelmButtons is the single hardware access point for the three physical
  buttons of the LG V60 (timelm):

      Power       -> PMIC PON  KPDPWR_N  (pm8150@0, qcom,power-on@800)
      Volume Down -> PMIC PON  RESIN_N   (pm8150@0, qcom,power-on@800)
      Volume Up   -> PMIC GPIO 6        (pm8150_gpios pinctrl@c000)

  The device binds to the existing ACPI node ACPI\KONA1003 (the PMIC node of
  the runtime DSDT), so no firmware / SSDT change is required.

--*/

#include "driver.h"
#include "driver.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, ButtonsEvtDeviceAdd)
#pragma alloc_text (PAGE, ButtonsEvtDriverContextCleanup)
#endif

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = ButtonsEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config,
                           ButtonsEvtDeviceAdd
                           );

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             WDF_NO_HANDLE
                             );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

NTSTATUS
ButtonsEvtDeviceAdd(
    _In_     WDFDRIVER        Driver,
    _Inout_  PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = ButtonsCreateDevice(DeviceInit);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit status %!STATUS!", status);

    return status;
}

VOID
ButtonsEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
