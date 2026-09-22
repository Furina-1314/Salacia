/**
  ******************************************************************************
  * @file    sensor_service.c
 * @brief   Sensor service for MPU6500 and asynchronous DYP measurements.
  ******************************************************************************
  */

#include "sensor_service.h"

#include "dyp.h"

static MPU6500_HandleTypeDef mpu6500;
static uint8_t mpu_ready;

static SensorService_StatusTypeDef SensorService_MapMpuStatus(
    MPU6500_StatusTypeDef status)
{
    switch (status)
    {
        case MPU6500_STATUS_OK:
            return SENSOR_SERVICE_STATUS_OK;

        case MPU6500_STATUS_BAD_ARG:
            return SENSOR_SERVICE_STATUS_BAD_ARG;

        case MPU6500_STATUS_NOT_READY:
            return SENSOR_SERVICE_STATUS_NOT_READY;

        case MPU6500_STATUS_BUSY:
            return SENSOR_SERVICE_STATUS_BUSY;

        case MPU6500_STATUS_TIMEOUT:
            return SENSOR_SERVICE_STATUS_TIMEOUT;

        default:
            return SENSOR_SERVICE_STATUS_IO_ERROR;
    }
}

SensorService_StatusTypeDef SensorService_Init(I2C_HandleTypeDef *i2c)
{
    SensorService_StatusTypeDef status;

    status = SensorService_MapMpuStatus(MPU6500_Init(&mpu6500, i2c));
    mpu_ready = (status == SENSOR_SERVICE_STATUS_OK) ? 1U : 0U;
    return status;
}

SensorService_StatusTypeDef SensorService_ReadMpuRaw(MPU6500_RawData *data)
{
    return SensorService_MapMpuStatus(MPU6500_ReadRaw(&mpu6500, data));
}

uint8_t SensorService_IsMpuReady(void)
{
    return mpu_ready;
}

uint8_t SensorService_GetMpuWhoAmI(void)
{
    return MPU6500_GetWhoAmI(&mpu6500);
}

uint32_t SensorService_GetMpuLastHalError(void)
{
    return MPU6500_GetLastHalError(&mpu6500);
}

static SensorService_StatusTypeDef SensorService_MapDypStatus(
    DYP_StatusTypeDef status)
{
    switch (status)
    {
        case DYP_STATUS_OK:
            return SENSOR_SERVICE_STATUS_OK;

        case DYP_STATUS_BAD_ARG:
            return SENSOR_SERVICE_STATUS_BAD_ARG;

        case DYP_STATUS_NOT_READY:
            return SENSOR_SERVICE_STATUS_NOT_READY;

        case DYP_STATUS_BUSY:
            return SENSOR_SERVICE_STATUS_BUSY;

        case DYP_STATUS_TIMEOUT:
            return SENSOR_SERVICE_STATUS_TIMEOUT;

        default:
            return SENSOR_SERVICE_STATUS_IO_ERROR;
    }
}

SensorService_StatusTypeDef SensorService_InitDyp(void)
{
    return SensorService_MapDypStatus(DYP_Init());
}

void SensorService_DeInitDyp(void)
{
    DYP_DeInit();
}

SensorService_StatusTypeDef SensorService_StartDypMeasurement(void)
{
    return SensorService_MapDypStatus(DYP_StartMeasurement());
}

void SensorService_Process(void)
{
    DYP_Process();
}

SensorService_StatusTypeDef SensorService_GetDypSnapshot(
    SensorService_DypSnapshotTypeDef *snapshot)
{
    DYP_SnapshotTypeDef dyp_snapshot;
    DYP_StatusTypeDef status;

    if (snapshot == NULL)
    {
        return SENSOR_SERVICE_STATUS_BAD_ARG;
    }

    status = DYP_GetSnapshot(&dyp_snapshot);
    if (status != DYP_STATUS_OK)
    {
        return SensorService_MapDypStatus(status);
    }

    snapshot->state = (SensorService_DypStateTypeDef)dyp_snapshot.state;
    snapshot->distance_mm = dyp_snapshot.distance_mm;
    snapshot->age_ms = dyp_snapshot.age_ms;
    snapshot->ready = dyp_snapshot.ready;
    snapshot->busy = dyp_snapshot.busy;
    snapshot->valid = dyp_snapshot.valid;
    return SENSOR_SERVICE_STATUS_OK;
}

uint32_t SensorService_GetDypLastHalError(void)
{
    return DYP_GetLastHalError();
}
