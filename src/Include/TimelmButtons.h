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
#define TIMELM_BUTTON_POWER       0
#define TIMELM_BUTTON_VOLUME_UP   1
#define TIMELM_BUTTON_VOLUME_DOWN 2
#define TIMELM_BUTTON_COUNT       3

#define TIMELM_BUTTON_BIT_POWER       0x00000001u
#define TIMELM_BUTTON_BIT_VOLUME_UP   0x00000002u
#define TIMELM_BUTTON_BIT_VOLUME_DOWN 0x00000004u

//
// Polarity of the sampled register bit.
//
#define TIMELM_POLARITY_HIGH 0u /* pressed when (value & Mask) != 0 */
#define TIMELM_POLARITY_LOW  1u /* pressed when (value & Mask) == 0 */

#define TIMELM_BUTTON_STATE_VERSION   1u
#define TIMELM_BUTTON_MAPPING_VERSION 1u

//
// Maximum number of bytes that can be transferred by one RAW_READ/DUMP.
// The SPMI arbiter supports at most 8 bytes per transaction.
//
#define TIMELM_BUTTONS_MAX_TRANSFER   8u

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
// Complete mapping of the three buttons.
//
typedef struct _TIMELM_BUTTON_MAPPING {
    UINT32 Version;   /* TIMELM_BUTTON_MAPPING_VERSION */
    UINT32 Count;     /* number of valid Source[] entries */
    TIMELM_BUTTON_SOURCE Source[TIMELM_BUTTON_COUNT];
} TIMELM_BUTTON_MAPPING, *PTIMELM_BUTTON_MAPPING;

//
// Runtime state of the buttons.
//
typedef struct _TIMELM_BUTTON_STATE {
    UINT32 Version;         /* TIMELM_BUTTON_STATE_VERSION */
    UINT32 Ready;           /* 1 when the PMIC bus is reachable */
    UINT32 Buttons;         /* bitmask of the buttons that are held down */
    UINT32 ChangeCounter;   /* incremented on every debounced edge */
    UINT32 PressCount[TIMELM_BUTTON_COUNT];
    UINT32 HeldMillis[TIMELM_BUTTON_COUNT]; /* duration of the current (or last) press */
    UINT32 LastEdgeMillis;  /* milliseconds since the driver was loaded */
    UINT32 Reserved;
} TIMELM_BUTTON_STATE, *PTIMELM_BUTTON_STATE;

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
