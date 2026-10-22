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
EVT_WDF_DEVICE_D0_ENTRY ButtonsEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT ButtonsEvtDeviceD0Exit;
EVT_WDF_OBJECT_CONTEXT_CLEANUP ButtonsEvtDriverContextCleanup;

EXTERN_C_END
