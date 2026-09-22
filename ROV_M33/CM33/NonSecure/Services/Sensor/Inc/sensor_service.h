/**
  ******************************************************************************
  * @file    sensor_service.h
 * @brief   Sensor service for MPU6500 and asynchronous DYP measurements.
  ******************************************************************************
  */

#ifndef SENSOR_SERVICE_H
#define SENSOR_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mpu6500.h"

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

typedef enum
{
    SENSOR_SERVICE_STATUS_OK = 0,
    SENSOR_SERVICE_STATUS_BAD_ARG,
    SENSOR_SERVICE_STATUS_NOT_READY,
    SENSOR_SERVICE_STATUS_BUSY,
    SENSOR_SERVICE_STATUS_TIMEOUT,
    SENSOR_SERVICE_STATUS_IO_ERROR
} SensorService_StatusTypeDef;

SensorService_StatusTypeDef SensorService_Init(I2C_HandleTypeDef *i2c);
SensorService_StatusTypeDef SensorService_ReadMpuRaw(MPU6500_RawData *data);
uint8_t SensorService_IsMpuReady(void);
uint8_t SensorService_GetMpuWhoAmI(void);
uint32_t SensorService_GetMpuLastHalError(void);

SensorService_StatusTypeDef SensorService_InitDyp(void);
void SensorService_DeInitDyp(void);
SensorService_StatusTypeDef SensorService_StartDypMeasurement(void);
void SensorService_Process(void);
SensorService_StatusTypeDef SensorService_GetDypSnapshot(
    SensorService_DypSnapshotTypeDef *snapshot);
uint32_t SensorService_GetDypLastHalError(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_SERVICE_H */
