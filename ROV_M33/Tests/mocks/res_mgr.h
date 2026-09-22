#ifndef TEST_RES_MGR_H
#define TEST_RES_MGR_H

#include <stdint.h>

typedef enum
{
    RESMGR_RESOURCE_RIFSC = 0,
    RESMGR_RESOURCE_RIF_RCC,
    RESMGR_RESOURCE_RIF_GPIOB
} ResMgr_Res_Type_t;

typedef enum
{
    RESMGR_STATUS_ACCESS_OK = 0,
    RESMGR_STATUS_ACCESS_ERROR
} ResMgr_Status_t;

#define STM32MP25_RIFSC_UART4_ID  34U
#define RESMGR_GPIO_PIN(pin)       ((uint8_t)(pin))
#define RESMGR_RCC_RESOURCE(id)    ((uint8_t)(id))

ResMgr_Status_t ResMgr_Request(ResMgr_Res_Type_t type, uint8_t resource);
ResMgr_Status_t ResMgr_Release(ResMgr_Res_Type_t type, uint8_t resource);

#endif
