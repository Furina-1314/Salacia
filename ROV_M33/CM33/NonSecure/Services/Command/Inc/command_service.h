/**
  ******************************************************************************
  * @file    command_service.h
  * @brief   Unified RPMsg command envelope parser and dispatcher.
  ******************************************************************************
  */

#ifndef COMMAND_SERVICE_H
#define COMMAND_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*CommandService_SendCallback)(const char *message);
typedef void (*CommandService_IoErrorCallback)(uint32_t hal_error);

void CommandService_Init(void);
void CommandService_Handle(const char *message,
                           CommandService_SendCallback send_callback,
                           CommandService_IoErrorCallback io_error_callback);
void CommandService_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_SERVICE_H */
