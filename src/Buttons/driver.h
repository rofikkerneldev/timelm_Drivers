/*++

Module Name:

  driver.h

Abstract:

  Declarations for the TimelmButtons KMDF driver (LG V60 / timelm).

--*/

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include <SPMI.h>
#include <TimelmButtons.h>

#include "device.h"
#include "queue.h"
#include "trace.h"

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD ButtonsEvtDeviceAdd;

//
// The power callbacks of the device are declared with a plain prototype.
// The typedef of the WDK is a pointer in this version of the headers, a
// declaration with the typedef would be a variable and would collide with
// the function itself.
//
NTSTATUS
ButtonsEvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

VOID
ButtonsEvtDeviceD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

EVT_WDF_OBJECT_CONTEXT_CLEANUP ButtonsEvtDriverContextCleanup;

EXTERN_C_END
