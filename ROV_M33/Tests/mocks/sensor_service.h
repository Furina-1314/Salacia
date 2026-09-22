#ifndef TEST_SENSOR_SERVICE_H
#define TEST_SENSOR_SERVICE_H

#include "mpu6500.h"

typedef enum
{
    SENSOR_SERVICE_STATUS_OK = 0,
    SENSOR_SERVICE_STATUS_BAD_ARG,
    SENSOR_SERVICE_STATUS_NOT_READY,
    SENSOR_SERVICE_STATUS_BUSY,
    SENSOR_SERVICE_STATUS_TIMEOUT,
    SENSOR_SERVICE_STATUS_IO_ERROR
} SensorService_StatusTypeDef;

typedef enum
{
    SENSOR_SERVICE_DYP_UNINITIALIZED = 0,
    SENSOR_SERVICE_DYP_IDLE,
    SENSOR_SERVICE_DYP_WAITING,
    SENSOR_SERVICE_DYP_COMPLETE,
    SENSOR_SERVICE_DYP_TIMEOUT,
    SENSOR_SERVICE_DYP_IO_ERROR
} SensorService_DypStateTypeDef;

typedef struct
{
    SensorService_DypStateTypeDef state;
    uint16_t distance_mm;
    uint32_t age_ms;
    uint8_t ready;
    uint8_t busy;
    uint8_t valid;
} SensorService_DypSnapshotTypeDef;

SensorService_StatusTypeDef SensorService_ReadMpuRaw(MPU6500_RawData *data);
uint8_t SensorService_IsMpuReady(void);
SensorService_StatusTypeDef SensorService_StartDypMeasurement(void);
SensorService_StatusTypeDef SensorService_GetDypSnapshot(
    SensorService_DypSnapshotTypeDef *snapshot);
uint32_t SensorService_GetDypLastHalError(void);

#endif
