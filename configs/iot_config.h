/*
 * Copyright 2020 Cypress Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file iot_config.h
 * @brief This file contains configuration settings for the MQTT Library.
 */

#ifndef IOT_CONFIG_H_
#define IOT_CONFIG_H_

#include "cy_iot_platform_types.h"

#include <assert.h>
#include <stdlib.h>

/**
 * @addtogroup mqtt_cyport_config
 * @{
 */
/**
 * @brief Default thread priority for the threads created by AWS IoT Device SDK.
 */
#define IOT_THREAD_DEFAULT_PRIORITY                 ( CY_RTOS_PRIORITY_NORMAL )
/**
 * @brief Default thread stack size for the threads created by AWS IoT Device SDK.
 * The stack size may be tuned to suit the desired use case.
 */
#define IOT_THREAD_DEFAULT_STACK_SIZE               ( 8192 )

/**
 * @brief Default wait time (in milliseconds) to receive keep-alive responses from the MQTT broker. This value may be adjusted to suit the use case and network environment.
 * Refer aws-iot-device-sdk-embedded-C/doc/lib/mqtt.txt for additional info.
 */
#define IOT_MQTT_RESPONSE_WAIT_MS                   ( 5000U )

/**
 * \cond
 * @brief Macros to enable/disable asserts in the IoT Device SDK library.
 * Asserts are disabled by default; to enable asserts, modify these macros to 1.
 */
#define IOT_CONTAINERS_ENABLE_ASSERTS               ( 0 )
#define IOT_MQTT_ENABLE_ASSERTS                     ( 0 )
#define IOT_TASKPOOL_ENABLE_ASSERTS                 ( 0 )
#define AWS_IOT_SHADOW_ENABLE_ASSERTS               ( 0 )
#define AWS_IOT_DEFENDER_ENABLE_ASSERTS             ( 0 )
#define AWS_IOT_JOBS_ENABLE_ASSERTS                 ( 0 )
/**
 * \endcond
 */

/**
 * @brief Insert program diagnostics. This function should have the same signature as [assert](https://pubs.opengroup.org/onlinepubs/9699919799/functions/assert.html)
 */
#if (IOT_CONTAINERS_ENABLE_ASSERTS == 1) || (IOT_MQTT_ENABLE_ASSERTS == 1) || (IOT_TASKPOOL_ENABLE_ASSERTS == 1) || (AWS_IOT_SHADOW_ENABLE_ASSERTS == 1) || (AWS_IOT_DEFENDER_ENABLE_ASSERTS == 1) || (AWS_IOT_JOBS_ENABLE_ASSERTS == 1)
#define Iot_DefaultAssert                           assert
#else
#define Iot_DefaultAssert
#endif

/**
 * @brief Memory allocation. This function should have the same signature as [malloc](http://pubs.opengroup.org/onlinepubs/9699919799/functions/malloc.html)
 */
#define Iot_DefaultMalloc                           malloc

/**
 * @brief Free memory. This function should have the same signature as [free](http://pubs.opengroup.org/onlinepubs/9699919799/functions/free.html)
 */
#define Iot_DefaultFree                             free

/**
 * \cond
 * @brief Library logging configuration. Configure the below macros to enable/disable debug logs in the library
 * Refer aws-iot-device-sdk-embedded-C/libraries/standard/common/include/iot_logging.h
 * for supported log levels.
 */
#define IOT_LOG_LEVEL_GLOBAL                        IOT_LOG_ERROR
#define IOT_LOG_LEVEL_DEMO                          IOT_LOG_ERROR
#define IOT_LOG_LEVEL_PLATFORM                      IOT_LOG_ERROR
#define IOT_LOG_LEVEL_NETWORK                       IOT_LOG_ERROR
#define IOT_LOG_LEVEL_TASKPOOL                      IOT_LOG_ERROR
#define IOT_LOG_LEVEL_MQTT                          IOT_LOG_ERROR
#define AWS_IOT_LOG_LEVEL_SHADOW                    IOT_LOG_ERROR
#define AWS_IOT_LOG_LEVEL_DEFENDER                  IOT_LOG_ERROR
#define AWS_IOT_LOG_LEVEL_JOBS                      IOT_LOG_ERROR
/**
 * \endcond
 */

/**
 * @}
 */

#endif /* ifndef IOT_CONFIG_H_ */
