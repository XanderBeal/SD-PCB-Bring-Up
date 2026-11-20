#pragma once
#include "usbpd_def.h"
typedef enum {
    USBPD_PWR_BELOWVSAFE0V,
    USBPD_PWR_VSAFE5V,
    USBPD_PWR_SNKDETACH
} USBPD_VBUSPOWER_STATUS;
uint8_t USBPD_PWR_IF_GetVBUSStatus(uint8_t port, USBPD_VBUSPOWER_STATUS status);
