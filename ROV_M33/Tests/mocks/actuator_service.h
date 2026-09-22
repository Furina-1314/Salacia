#ifndef TEST_ACTUATOR_SERVICE_H
#define TEST_ACTUATOR_SERVICE_H

#include <stdint.h>

typedef enum
{
    ACTUATOR_SERVICE_STATUS_OK = 0,
    ACTUATOR_SERVICE_STATUS_BAD_ARG,
    ACTUATOR_SERVICE_STATUS_NOT_READY,
    ACTUATOR_SERVICE_STATUS_IO_ERROR
} ActuatorService_StatusTypeDef;

#define ACTUATOR_SERVICE_SERVO_CHANNEL_COUNT 10U
#define ACTUATOR_SERVICE_SERVO_CHANNEL_LAST   9U
#define ACTUATOR_SERVICE_SERVO_MAX_ANGLE     180U

ActuatorService_StatusTypeDef ActuatorService_SetServo(uint8_t id,
                                                       uint8_t angle);
ActuatorService_StatusTypeDef ActuatorService_SetServoAll(uint8_t angle);
ActuatorService_StatusTypeDef ActuatorService_GetServo(uint8_t id,
                                                       uint8_t *angle);
ActuatorService_StatusTypeDef ActuatorService_GetServoAll(uint8_t *angles);
ActuatorService_StatusTypeDef ActuatorService_SetServoMid(uint8_t id);
ActuatorService_StatusTypeDef ActuatorService_SetServoAllMid(void);

ActuatorService_StatusTypeDef ActuatorService_SetChannelPulseUs(
    uint8_t first_channel,
    const uint16_t *pulse_us,
    uint8_t count);
uint32_t ActuatorService_GetLastHalError(void);

#endif
