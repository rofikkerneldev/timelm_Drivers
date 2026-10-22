/*++

Module Name:

  device.h

Abstract:

  Device context and helpers for TimelmButtons.sys.

--*/

#pragma once

#include <SPMI.h>
#include <TimelmButtons.h>
#include <vhf.h>

EXTERN_C_START

//
// SPMI slave id of the primary PMIC (pm8150@0) on the LG V60 / timelm.
// Verified against the live device tree (diagnostics/recovery.dts).
//
#define TIMELM_PMIC_SID_PRIMARY        0x00

//
// Power-on block, node qcom,power-on@800.
// Offsets follow upstream Linux drivers/input/misc/qpnp-power-on.c:
//   QPNP_PON_RT_STS = base + 0x10   (live status of the two keys)
//   bit0 = KPDPWR_N (power key), bit1 = RESIN_N (volume down key)
//
#define TIMELM_PON_BASE                0x0800
#define TIMELM_PON_RT_STS              (TIMELM_PON_BASE + 0x10) /* 0x810 */
#define TIMELM_PON_KPDPWR_N_MASK       0x01
#define TIMELM_PON_RESIN_N_MASK        0x02

//
// PMIC GPIO block, node pm8150_gpios pinctrl@c000.
// Offsets follow upstream Linux drivers/pinctrl/qcom/pinctrl-spmi-gpio.c:
//   PMIC_GPIO_ADDRESS_RANGE = 0x100 per GPIO pad
//   PMIC_MPP_REG_RT_STS      = pad + 0x10 (input level, mask 0x1)
// The live device tree maps gpio N to pad base 0xC000 + (N - 1) * 0x100
// (gpio1 -> 0xC000 ... gpio6 -> 0xC500), confirmed by the interrupt numbers
// (gpio N -> 0xC0 + (N - 1)).
// gpio_keys "vol_up" uses gpio6 and is active low.
//
#define TIMELM_PMIC_GPIO_BASE          0xC000
#define TIMELM_PMIC_GPIO_STRIDE        0x100
#define TIMELM_PMIC_GPIO_MPP_RT_STS    0x10
#define TIMELM_PMIC_GPIO_VOL_UP_INDEX  6
#define TIMELM_PMIC_GPIO_VOL_UP_ADDR \
    (TIMELM_PMIC_GPIO_BASE + ((TIMELM_PMIC_GPIO_VOL_UP_INDEX - 1) * TIMELM_PMIC_GPIO_STRIDE) + TIMELM_PMIC_GPIO_MPP_RT_STS)

//
// Sampling parameters.
//
#define TIMELM_BUTTONS_POLL_PERIOD_MS   20
#define TIMELM_BUTTONS_DEBOUNCE_SAMPLES 2
#define TIMELM_BUTTONS_SYMBOLIC_LINK    L"\\DosDevices\\TimelmButtons"

//
// Relative timeout in 100 ns units, the unit that WdfTimerStart expects.
//
#define TIMELM_BUTTONS_REL_TIMEOUT_MS(_ms) (-((LONGLONG)(_ms) * 10000))

//
// Per button debounce / edge tracking.
//
typedef struct _BUTTON_ENTRY {
    BOOLEAN Candidate;
    BOOLEAN Stable;
    UINT16 CandidateCount;
    UINT32 PressCount;
    ULONG64 PressStartMs;
    ULONG64 LastEdgeMs;
    ULONG64 HeldMs;
} BUTTON_ENTRY, *PBUTTON_ENTRY;

typedef struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFTIMER Timer;
    WDFWORKITEM WorkItem;
    WDFIOTARGET IoTarget;
    SPMI_INTERFACE SPMIInterface;
    BOOLEAN SpmiReady;
    WDFSPINLOCK Lock;
    TIMELM_BUTTON_MAPPING Mapping;
    TIMELM_BUTTON_STATE State;
    BUTTON_ENTRY Entries[TIMELM_BUTTON_COUNT];
    KEVENT ChangeEvent;
    ULONG64 StartMs;
    ULONG RetryCounter;
    VHFHANDLE HidHandle;
    BOOLEAN HidStarted;
    BOOLEAN HidStopping;
    LONG HidBusy;
    KEVENT HidIdleEvent;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

NTSTATUS
ButtonsCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

NTSTATUS
ButtonsAcquireSpmi(
    _In_ WDFDEVICE Device
    );

VOID
ButtonsLoadDefaultMapping(
    _Out_ PTIMELM_BUTTON_MAPPING Mapping
    );

NTSTATUS
ButtonsReadPmic(
    _In_ PDEVICE_CONTEXT Context,
    _In_ UINT32 Sid,
    _In_ UINT32 Addr,
    _Out_writes_bytes_opt_(Length) PUINT8 Data,
    _In_ UINT32 Length
    );

VOID
ButtonsSample(
    _Inout_ PDEVICE_CONTEXT Context
    );

//
// The HID face of the driver.  The device is created with the Virtual HID
// Framework, so that Windows sees the buttons as a real HID device.  A
// failure of the HID face never stops the driver, the agent path stays
// usable.
//
NTSTATUS
ButtonsHidInitialize(
    _Inout_ PDEVICE_CONTEXT Context
    );

NTSTATUS
ButtonsHidStart(
    _Inout_ PDEVICE_CONTEXT Context
    );

//
// Deletes the virtual HID device.  It waits for a running report to finish,
// so that the handle is not used while it is deleted.  It is called when the
// device is removed, otherwise the driver unload would leave the virtual
// device behind.
//
VOID
ButtonsHidStop(
    _Inout_ PDEVICE_CONTEXT Context
    );

VOID
ButtonsHidReport(
    _Inout_ PDEVICE_CONTEXT Context,
    _In_ UINT32 HidAction
    );

//
// Decides what happens with a button that was released again.  A short press
// belongs to the HID face or to the agent, a long press always belongs to the
// agent, because only the agent knows the long actions.
//
VOID
ButtonsHandleRelease(
    _Inout_ PDEVICE_CONTEXT Context,
    _In_ UINT32 Index,
    _In_ UINT32 HeldMillis
    );

EVT_WDF_TIMER ButtonsEvtTimerFired;

EVT_WDF_WORKITEM ButtonsEvtWorkItem;

ULONG64
ButtonsNowMs(
    _In_ PDEVICE_CONTEXT Context
    );

EXTERN_C_END
