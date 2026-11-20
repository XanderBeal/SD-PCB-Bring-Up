#include <string.h>
#include "usbpd_core.h"
#include "usbpd_hw_if.h"
#include "usbpd_timersserver.h"
#include "pd_board.h"

extern uint32_t HAL_GetTick(void);
static USBPD_ParamsTypeDef params;
static USBPD_SettingsTypeDef settings = {
    .PE_SupportedSOP = USBPD_SUPPORTED_SOP_SOP,
    .PE_SpecRevision = USBPD_SPECIFICATION_REV3,
    .PE_DefaultRole = USBPD_PORTPOWERROLE_SNK,
    .PE_RoleSwap = USBPD_FALSE,
    .CAD_RoleToggle = USBPD_FALSE,
    .CAD_DefaultResistor = 1,
};
static uint32_t sourcePdos[7];
static uint32_t sourceCount;
static uint32_t requestedMv = 5000;
static volatile uint8_t attached;
static volatile uint8_t initialized;
static uint32_t lastTick;
static volatile uint8_t cadWake = 1;
static volatile uint8_t peWake = 1;
static uint32_t cadLastRun, peLastRun;
static uint32_t cadDelay, peDelay;

static void wake(void) { cadWake = 1; }
static void wakePe(uint8_t port) { (void)port; peWake = 1; }

static void cableEvent(uint8_t port, USBPD_CAD_EVENT event, CCxPin_TypeDef cc) {
    (void)cc;
    sourceCount = 0;
    requestedMv = 5000;
    if (event == USBPD_CAD_EVENT_ATTACHED || event == USBPD_CAD_EVENT_ATTEMC) {
        attached = 1;
        USBPD_PE_StateMachine_Reset(port);
        peWake = 1;
    } else if (event == USBPD_CAD_EVENT_DETACHED || event == USBPD_CAD_EVENT_EMC) {
        attached = 0;
        USBPD_PE_StateMachine_Stop(port);
        params.PE_Power = USBPD_POWER_NO;
    }
}

static void getData(uint8_t port, USBPD_CORE_DataInfoType_TypeDef id,
                    uint8_t *out, uint32_t *size) {
    (void)port;
    *size = 0;
    if (id == USBPD_CORE_DATATYPE_SNK_PDO) {
        // Fixed sink PDOs: 5 V / 1 A mandatory fallback; 20 V / 1 A target.
        // Higher capability flag is set in the first PDO.
        const uint32_t sinkPdos[] = {
            (1u << 28) | (100u << 10) | 100u,
            (400u << 10) | 100u
        };
        memcpy(out, sinkPdos, sizeof(sinkPdos));
        *size = sizeof(sinkPdos);
    } else if (id == USBPD_CORE_DATATYPE_REQ_VOLTAGE) {
        memcpy(out, &requestedMv, sizeof(requestedMv));
        *size = sizeof(requestedMv);
    }
}

static void setData(uint8_t port, USBPD_CORE_DataInfoType_TypeDef id,
                    uint8_t *data, uint32_t size) {
    (void)port;
    if (id == USBPD_CORE_DATATYPE_RCV_SRC_PDO) {
        sourceCount = 0;
        if (size && size <= sizeof(sourcePdos) && size % 4 == 0) {
            memcpy(sourcePdos, data, size);
            sourceCount = size / 4;
        }
    }
}

static void evaluate(uint8_t port, uint32_t *rdo, USBPD_CORE_PDO_Type_TypeDef *type) {
    (void)port;
    uint32_t position = 0;
    uint32_t current = 100; // Fixed RDO current unit is 10 mA.
    uint32_t mismatch = 1;
    requestedMv = 5000;
    for (uint32_t i = 0; i < sourceCount; ++i) {
        const uint32_t pdo = sourcePdos[i];
        if ((pdo >> 30) == 0 && ((pdo >> 10) & 0x3ff) == 400 &&
            (pdo & 0x3ff) >= 100) {
            position = i;
            requestedMv = 20000;
            mismatch = 0;
            break;
        }
    }
    if (mismatch && sourceCount) {
        // A compliant source starts its list with a fixed 5 V PDO.
        const uint32_t available = sourcePdos[0] & 0x3ff;
        if (current > available) current = available;
    }
    *type = USBPD_CORE_PDO_TYPE_FIXED;
    // No USB data capability or role swaps advertised. NoUSB suspend is set.
    *rdo = ((position + 1) << 28) | (mismatch << 26) | (1u << 24) |
           (current << 10) | current;
}

static void notify(uint8_t port, USBPD_NotifyEventValue_TypeDef event) {
    (void)port; (void)event;
}
static void hardReset(uint8_t port, USBPD_PortPowerRole_TypeDef role,
                      USBPD_HR_Status_TypeDef status) {
    (void)port; (void)role;
    if (status == USBPD_HR_STATUS_START_ACK || status == USBPD_HR_STATUS_START_REQ)
        requestedMv = 5000;
}
static void extended(uint8_t port, USBPD_ExtendedMsg_TypeDef id,
                     uint8_t *data, uint16_t size) {
    (void)port; (void)id; (void)data; (void)size;
}
static void recovery(uint8_t port) { USBPD_CAD_EnterErrorRecovery(port); }
static USBPD_FunctionalState powerReady(uint8_t port, USBPD_VSAFE_StatusTypeDef safe) {
    (void)port;
    // Divider-less bring-up: use the TCPP's VBUS presence comparator for
    // reset recovery. This does not measure the negotiated voltage.
    const int present = pd_board_present();
    return (safe == USBPD_VSAFE_0V ? !present : present) ? USBPD_ENABLE : USBPD_DISABLE;
}

uint8_t USBPD_PWR_IF_GetVBUSStatus(uint8_t port, USBPD_VBUSPOWER_STATUS status) {
    (void)port;
    const int present = pd_board_present();
    return status == USBPD_PWR_VSAFE5V ? present : !present;
}

int pd_app_init(void) {
    static const USBPD_CAD_Callbacks cad = { cableEvent, wake };
    static const USBPD_PE_Callbacks pe = {
        .USBPD_PE_HardReset = hardReset,
        .USBPD_PE_Notify = notify,
        .USBPD_PE_ExtendedMessage = extended,
        .USBPD_PE_GetDataInfo = getData,
        .USBPD_PE_SetDataInfo = setData,
        .USBPD_PE_SNK_EvaluateCapabilities = evaluate,
        .USBPD_PE_WakeupCallback = wakePe,
        .USBPD_PE_EnterErrorRecovery = recovery,
        .USBPD_PE_IsPowerReady = powerReady,
    };
    if (!USBPD_PE_CheckLIB(LIB_ID)) return 0;
    __HAL_RCC_UCPD1_CLK_ENABLE();
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    USBPD_HW_IF_GlobalHwInit();
    params.PE_SpecRevision = settings.PE_SpecRevision;
    params.PE_PowerRole = USBPD_PORTPOWERROLE_SNK;
    if (USBPD_CAD_Init(0, &cad, &settings, &params) != USBPD_CAD_OK) return 0;
    if (USBPD_PE_Init(0, &settings, &params, &pe) != USBPD_OK) return 0;
    params.DPM_Initialized = USBPD_TRUE;
    lastTick = HAL_GetTick();
    initialized = 1;
    USBPD_CAD_PortEnable(0, USBPD_CAD_ENABLE);
    return 1;
}

void pd_app_tick(void) {
    if (!initialized) return;
    USBPD_PE_TimerCounter(0);
    USBPD_PRL_TimerCounter(0);
}
void pd_app_run(void) {
    const uint32_t now = HAL_GetTick();
    while (lastTick != now) { ++lastTick; pd_app_tick(); }
    if (cadWake || (cadDelay != UINT32_MAX && now - cadLastRun >= cadDelay)) {
        cadWake = 0;
        cadLastRun = now;
        cadDelay = USBPD_CAD_Process();
    }
    if (attached && (peWake || (peDelay != UINT32_MAX && now - peLastRun >= peDelay))) {
        peWake = 0;
        peLastRun = now;
        peDelay = USBPD_PE_StateMachine_SNK(0);
        // FLGn is unconnected on this board. Poll for VBUS changes instead of
        // waiting indefinitely for a hardware VBUS-detect wake-up callback.
        if (peDelay > 1) peDelay = 1;
    }
}

void UCPD1_IRQHandler(void) { USBPD_PORT0_IRQHandler(); }
