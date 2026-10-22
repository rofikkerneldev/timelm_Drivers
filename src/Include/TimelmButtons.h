#pragma once

//
// Public contract for TimelmButtons.sys (LG V60 / timelm bring-up).
//
// The same layout is used by the kernel driver (src/Buttons) and by the
// user mode agent (mu-timelm/Resources/Scripts/Buttons).
//
// Every member is a 32 bit unsigned integer so that the structure layout is
// identical on ARM64 and x64.
//

#define TIMELM_BUTTONS_DEVICE_TYPE     FILE_DEVICE_UNKNOWN
#define TIMELM_BUTTONS_FN_GET_STATE    0x800
#define TIMELM_BUTTONS_FN_RAW_READ     0x801
#define TIMELM_BUTTONS_FN_SET_MAPPING  0x802
#define TIMELM_BUTTONS_FN_GET_MAPPING  0x803
#define TIMELM_BUTTONS_FN_DUMP         0x804
#define TIMELM_BUTTONS_FN_WAIT         0x805

//
// Numeric values (for callers that cannot use CTL_CODE):
//   GET_STATE   0x226000
//   RAW_READ    0x226004
//   SET_MAPPING 0x22A008
//   GET_MAPPING 0x22600C
//   DUMP        0x226010
//   WAIT        0x226014
//
#define IOCTL_TIMELM_BUTTONS_GET_STATE \
    CTL_CODE(TIMELM_BUTTONS_DEVICE_TYPE, TIMELM_BUTTONS_FN_GET_STATE, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_TIMELM_BUTTONS_RAW_READ \
    CTL_CODE(TIMELM_BUTTONS_DEVICE_TYPE, TIMELM_BUTTONS_FN_RAW_READ, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_TIMELM_BUTTONS_SET_MAPPING \
    CTL_CODE(TIMELM_BUTTONS_DEVICE_TYPE, TIMELM_BUTTONS_FN_SET_MAPPING, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_TIMELM_BUTTONS_GET_MAPPING \
    CTL_CODE(TIMELM_BUTTONS_DEVICE_TYPE, TIMELM_BUTTONS_FN_GET_MAPPING, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_TIMELM_BUTTONS_DUMP \
    CTL_CODE(TIMELM_BUTTONS_DEVICE_TYPE, TIMELM_BUTTONS_FN_DUMP, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_TIMELM_BUTTONS_WAIT \
    CTL_CODE(TIMELM_BUTTONS_DEVICE_TYPE, TIMELM_BUTTONS_FN_WAIT, METHOD_BUFFERED, FILE_READ_ACCESS)

//
// Button indices.
//
// The first three indices are the ones of the first version of the contract,
// the fourth one is the assist key of the phone (the "hot1" key of the
// device tree, gpio1 of the PMIC).
//
#define TIMELM_BUTTON_POWER       0
#define TIMELM_BUTTON_VOLUME_UP   1
#define TIMELM_BUTTON_VOLUME_DOWN 2
#define TIMELM_BUTTON_ASSIST      3

//
// Number of buttons of the first version of the contract and the number of
// buttons of the current version.
//
#define TIMELM_BUTTON_LEGACY_COUNT 3
#define TIMELM_BUTTON_COUNT       4

#define TIMELM_BUTTON_BIT_POWER       0x00000001u
#define TIMELM_BUTTON_BIT_VOLUME_UP   0x00000002u
#define TIMELM_BUTTON_BIT_VOLUME_DOWN 0x00000004u
#define TIMELM_BUTTON_BIT_ASSIST      0x00000008u

//
// Polarity of the sampled register bit.
//
#define TIMELM_POLARITY_HIGH 0u /* pressed when (value & Mask) != 0 */
#define TIMELM_POLARITY_LOW  1u /* pressed when (value & Mask) == 0 */

#define TIMELM_BUTTON_STATE_VERSION   3u
#define TIMELM_BUTTON_MAPPING_VERSION 3u

//
// Size of the first version of the mapping structure.  A tool that still
// speaks that version is still accepted, the driver fills the actions with
// the defaults.  The first 80 bytes of the structure did not change, so a
// caller of the first version reads the same sources as before.
//
#define TIMELM_BUTTON_MAPPING_LEGACY_SIZE 80u

//
// Size of the second version of the mapping structure (three buttons with
// their actions).  The driver still accepts it.
//
#define TIMELM_BUTTON_MAPPING_V2_SIZE 152u

//
// Size of the current version of the mapping structure (four buttons).
//
#define TIMELM_BUTTON_MAPPING_SIZE 200u

//
// Size of the first version of the state structure.  The first 48 bytes did
// not change, so a caller of the first version reads the same state as
// before.
//
#define TIMELM_BUTTON_STATE_LEGACY_SIZE 48u

//
// Size of the second version of the state structure.
//
#define TIMELM_BUTTON_STATE_V2_SIZE 56u

//
// Size of the current version of the state structure (four buttons).
//
#define TIMELM_BUTTON_STATE_SIZE 64u

//
// Maximum number of bytes that can be transferred by one RAW_READ/DUMP.
// The SPMI arbiter supports at most 8 bytes per transaction.
//
#define TIMELM_BUTTONS_MAX_TRANSFER   8u

//
// What the driver reports to Windows as a HID device.  The HID face is
// optional, it is switched on per button with HidEnabled.
//
#define TIMELM_HID_NONE        0u /* the button is not reported as HID  */
#define TIMELM_HID_VOLUME_INC  1u /* consumer control, volume increment */
#define TIMELM_HID_VOLUME_DEC  2u /* consumer control, volume decrement */
#define TIMELM_HID_POWER_DOWN  3u /* system control, system power down   */

//
// What the user mode agent does with a button.  The agent and the HID face
// are two independent consumers of the same press, the mapping decides which
// of them reacts to a short press and which one reacts to a long press.
//
// The ids 0 .. 5 are the ones of the first version of the contract, the
// agent of the first version ignores the newer ones.
//
#define TIMELM_AGENT_NONE          0u
#define TIMELM_AGENT_TAB           1u
#define TIMELM_AGENT_ENTER         2u
#define TIMELM_AGENT_SHIFT_F10     3u
#define TIMELM_AGENT_LOCK          4u
#define TIMELM_AGENT_SHUTDOWN      5u
#define TIMELM_AGENT_WAKE          6u /* turn the display on again         */
#define TIMELM_AGENT_LOCK_OR_WAKE  7u /* lock when awake, wake when locked */
#define TIMELM_AGENT_LAUNCH_TARGET 8u /* start the program of the target file */
#define TIMELM_AGENT_START_MENU    9u /* open the start menu of Windows    */

//
// Describes where the state of one button is read from.
//
typedef struct _TIMELM_BUTTON_SOURCE {
    UINT32 Sid;       /* SPMI slave id (0 = pm8150 primary)     */
    UINT32 Addr;      /* PMIC register address (16 bit)         */
    UINT32 Mask;      /* bit mask inside the register byte      */
    UINT32 Polarity;  /* TIMELM_POLARITY_HIGH / _LOW            */
    UINT32 Enabled;   /* 0 = skip this source                    */
    UINT32 Reserved;
} TIMELM_BUTTON_SOURCE, *PTIMELM_BUTTON_SOURCE;

//
// What happens with one button.
//
typedef struct _TIMELM_BUTTON_ACTION {
    UINT32 HidAction;        /* TIMELM_HID_*                      */
    UINT32 LongPressMillis;  /* 0 = the button has no long press  */
    UINT32 AgentShortAction; /* TIMELM_AGENT_*, 0 = ignore        */
    UINT32 AgentLongAction;  /* TIMELM_AGENT_*, 0 = ignore        */
    UINT32 HidEnabled;       /* 0 = the HID face ignores it       */
    UINT32 Reserved;
} TIMELM_BUTTON_ACTION, *PTIMELM_BUTTON_ACTION;

//
// Complete mapping of the buttons.
//
// The layout is append only: the first 152 bytes are byte for byte the
// mapping of the second version of the contract (three sources at offset 8
// and three actions at offset 80).  The fourth button (the assist key) was
// appended behind them, so a tool that speaks the second version still
// reads the same sources and actions as before.
//
typedef struct _TIMELM_BUTTON_MAPPING {
    UINT32 Version;   /* TIMELM_BUTTON_MAPPING_VERSION */
    UINT32 Count;     /* number of valid buttons (4 in the current version) */
    TIMELM_BUTTON_SOURCE Source[TIMELM_BUTTON_LEGACY_COUNT];
    TIMELM_BUTTON_ACTION Action[TIMELM_BUTTON_LEGACY_COUNT];
    TIMELM_BUTTON_SOURCE SourceAssist;  /* the fourth button */
    TIMELM_BUTTON_ACTION ActionAssist;  /* the fourth button */
} TIMELM_BUTTON_MAPPING, *PTIMELM_BUTTON_MAPPING;

//
// Runtime state of the buttons.
//
// The layout is append only as well: the first 56 bytes are byte for byte
// the state of the second version of the contract.  The counters of the
// fourth button were appended behind them.
//
typedef struct _TIMELM_BUTTON_STATE {
    UINT32 Version;         /* TIMELM_BUTTON_STATE_VERSION */
    UINT32 Ready;           /* 1 when the PMIC bus is reachable */
    UINT32 Buttons;         /* bitmask of the buttons that are held down */
    UINT32 ChangeCounter;   /* incremented on every debounced edge */
    UINT32 PressCount[TIMELM_BUTTON_LEGACY_COUNT];
    UINT32 HeldMillis[TIMELM_BUTTON_LEGACY_COUNT]; /* duration of the current (or last) press */
    UINT32 LastEdgeMillis;  /* milliseconds since the driver was loaded */
    UINT32 HidReady;        /* 1 when the HID device is running (was Reserved in version 1) */
    UINT32 HidReportCount;  /* number of HID reports that were sent */
    UINT32 Reserved;
    UINT32 PressCountAssist; /* the fourth button */
    UINT32 HeldMillisAssist; /* the fourth button */
} TIMELM_BUTTON_STATE, *PTIMELM_BUTTON_STATE;

//
// Accessors for the fourth button.  The first three buttons keep their
// place in the arrays of the first version, the fourth one lives in its own
// member.  The macros return a pointer, so they can be read and written.
//
#define TIMELM_MAPPING_SOURCE(Mapping, Index) \
    (((Index) < TIMELM_BUTTON_LEGACY_COUNT) \
        ? &(Mapping)->Source[(Index)] : &(Mapping)->SourceAssist)

#define TIMELM_MAPPING_ACTION(Mapping, Index) \
    (((Index) < TIMELM_BUTTON_LEGACY_COUNT) \
        ? &(Mapping)->Action[(Index)] : &(Mapping)->ActionAssist)

#define TIMELM_STATE_PRESS_COUNT(State, Index) \
    (((Index) < TIMELM_BUTTON_LEGACY_COUNT) \
        ? &(State)->PressCount[(Index)] : &(State)->PressCountAssist)

#define TIMELM_STATE_HELD_MILLIS(State, Index) \
    (((Index) < TIMELM_BUTTON_LEGACY_COUNT) \
        ? &(State)->HeldMillis[(Index)] : &(State)->HeldMillisAssist)

//
// Input of IOCTL_TIMELM_BUTTONS_RAW_READ / IOCTL_TIMELM_BUTTONS_DUMP.
// The output buffer carries the same structure followed by Length data bytes.
//
typedef struct _TIMELM_BUTTON_RAW_REQUEST {
    UINT32 Sid;
    UINT32 Addr;
    UINT32 Length;   /* 1 .. TIMELM_BUTTONS_MAX_TRANSFER */
    UINT32 Reserved;
} TIMELM_BUTTON_RAW_REQUEST, *PTIMELM_BUTTON_RAW_REQUEST;

//
// Input of IOCTL_TIMELM_BUTTONS_WAIT, output is a TIMELM_BUTTON_STATE.
//
typedef struct _TIMELM_BUTTON_WAIT_REQUEST {
    UINT32 TimeoutMillis; /* 0 .. 5000 */
    UINT32 Reserved;
} TIMELM_BUTTON_WAIT_REQUEST, *PTIMELM_BUTTON_WAIT_REQUEST;
