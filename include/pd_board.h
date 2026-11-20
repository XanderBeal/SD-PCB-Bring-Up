#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int32_t BSP_USBPD_PWR_Init(uint32_t port);
int32_t BSP_USBPD_PWR_VBUSInit(uint32_t port);
int32_t BSP_USBPD_PWR_VBUSDeInit(uint32_t port);
int32_t BSP_USBPD_PWR_SetRole(uint32_t port, uint32_t role);
int32_t BSP_USBPD_PWR_VCCSetState(uint32_t port, uint32_t state);
int pd_board_present(void);
int pd_board_init(void);
int pd_app_init(void);
void pd_app_run(void);
void pd_app_tick(void);
#define POWER_ROLE_SINK 0u
#define BSP_ERROR_NONE 0
#ifdef __cplusplus
}
#endif
