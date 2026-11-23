#pragma once
#ifndef _KERNEL_MODE
#include <winioctl.h>
#endif

// Versioned, fixed-size payload. Axes use XInput polarity and range.
#define TANITA_SUBMIT_V1 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)
#define TANITA_DEVICE_PATH L"\\\\.\\TanitaGamepad"

typedef struct TanitaReport {
    unsigned short buttons;
    unsigned char leftTrigger;
    unsigned char rightTrigger;
    short leftX;
    short leftY;
    short rightX;
    short rightY;
} TanitaReport;
