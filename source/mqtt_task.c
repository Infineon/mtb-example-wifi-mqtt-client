/******************************************************************************
* File Name:   mqtt_task.c
*
* Description: This file contains the task that handles initialization & 
*              connection of Wi-Fi and the MQTT client. The task then starts 
*              the subscriber and the publisher tasks. The task also handles
*              all the cleanup operations to gracefully terminate the Wi-Fi and
*              MQTT connections in case of any failure.
*
* Related Document: See README.md
*
*******************************************************************************
* (c) 2020, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/

#include "cyhal.h"
#include "cybsp.h"

/* FreeRTOS header files */
#include "FreeRTOS.h"
#include "task.h"

/* Task header files */
#include "mqtt_task.h"
#include "subscriber_task.h"
#include "publisher_task.h"

/* Configuration file for Wi-Fi and MQTT client */
#include "wifi_config.h"
#include "mqtt_client_config.h"

/* Middleware libraries */
#include "cy_retarget_io.h"
#include "cy_wcm.h"
#include "cy_lwip.h"
#include "cy_iot_network_secured_socket.h"
#include "iot_init.h"
#include "iot_clock.h"

/******************************************************************************
* Macros
******************************************************************************/
/* Queue length of a message queue that is used to communicate the status of 
 * various operations.
 */
#define MQTT_STATUS_QUEUE_LENGTH         (1u)

/* Time in milliseconds to wait before creating the publisher task. */
#define TASK_CREATION_DELAY_MS           (2000u)

/* Flag Masks for tracking which cleanup functions must be called. */
#define WCM_INITIALIZED                  (1lu << 0)
#define WIFI_CONNECTED                   (1lu << 1)
#define IOT_SDK_INITIALIZED              (1lu << 2)
#define NETWORK_STACK_INITIALIZED        (1lu << 3)
#define LIBS_INITIALIZED                 (1lu << 4)
#define CONNECTION_ESTABLISHED           (1lu << 5)

/* Macro to check if the result of an operation was successful and set the 
 * corresponding bit in the init_flag based on 'init_mask' parameter. When 
 * it has failed, print the error message and return EXIT_FAILURE to the 
 * calling function.
 */
#define CHECK_RESULT(result, init_mask, error_message...)   \
                     do                                     \
                     {                                      \
                         if ((int)result == EXIT_SUCCESS)   \
                         {                                  \
                             init_flag |= init_mask;        \
                         }                                  \
                         else                               \
                         {                                  \
                             printf(error_message);         \
                             return EXIT_FAILURE;           \
                         }                                  \
                     } while(0)

/******************************************************************************
* Global Variables
*******************************************************************************/
/* MQTT connection handle. */
IotMqttConnection_t mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;

/* Queue handle used to communicate results of various operations - MQTT 
 * Publish, MQTT Subscribe, and MQTT connection between tasks and callbacks.
 */
QueueHandle_t mqtt_status_q;

/* Flag to denote initialization status of various operations. */
uint32_t init_flag;

/******************************************************************************
* Function Prototypes
*******************************************************************************/
static int wifi_connect(void);
static int mqtt_connect(void);
static int mqtt_get_unique_client_identifier(char *mqtt_client_identifier);
static void mqtt_disconnect_callback(void *pCallbackContext,
                                     IotMqttCallbackParam_t *pCallbackParam);
static void cleanup(void);

/******************************************************************************
 * Function Name: mqtt_client_task
 ******************************************************************************
 * Summary:
 *  Task for handling initialization & connection of Wi-Fi and the MQTT client.
 *  The task also creates and manages the subscriber and publisher tasks upon 
 *  successful MQTT connection.
 *
 * Parameters:
 *  void *pvParameters : Task parameter defined during task creation (unused)
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void mqtt_client_task(void *pvParameters)
{
    /* Structure that stores the data received over MQTT status queue from 
     * other tasks and callbacks.
     */
    mqtt_result_t mqtt_status;

    /* To avoid compiler warnings */
    (void)pvParameters;

    /* Create a message queue to communicate MQTT operation results 
     * between various tasks and callbacks.
     */
    mqtt_status_q = xQueueCreate(MQTT_STATUS_QUEUE_LENGTH, sizeof(mqtt_result_t));

    /* Initialize the Wi-Fi STA, connect to the Wi-Fi AP, set up the MQTT 
     * client, and connect to the MQTT broker. Jump to the cleanup block if 
     * any of the operations fails.
     */
    if ( (wifi_connect() != EXIT_SUCCESS) || (mqtt_connect() != EXIT_SUCCESS) )
    {
        goto exit_cleanup;
    }

    /* Create the subscriber task and cleanup if the operation fails. */
    if (pdPASS != xTaskCreate(subscriber_task, "Subscriber task", SUBSCRIBER_TASK_STACK_SIZE,
                              NULL, SUBSCRIBER_TASK_PRIORITY, &subscriber_task_handle))
    {
        printf("Failed to create the Subscriber task!\n");
        goto exit_cleanup;
    }

    /* Wait for the subscribe operation to complete. */
    vTaskDelay(pdMS_TO_TICKS(TASK_CREATION_DELAY_MS));

    /* Create the publisher task and cleanup if the operation fails. */
    if (pdPASS != xTaskCreate(publisher_task, "Publisher task", PUBLISHER_TASK_STACK_SIZE, 
                              NULL, PUBLISHER_TASK_PRIORITY, &publisher_task_handle))
    {
        printf("Failed to create Publisher task!\n");
        goto exit_cleanup;
    }

    while (true)
    {
        /* Wait for results of MQTT operations from other tasks and callbacks. */
        if (pdTRUE == xQueueReceive(mqtt_status_q, &mqtt_status, portMAX_DELAY))
        {
            switch(mqtt_status)
            {
                case MQTT_PUBLISH_FAILURE:
                {
                    /* Unsubscribe from the topic before cleanup. */
                    mqtt_unsubscribe();
                }
                case MQTT_SUBSCRIBE_FAILURE:
                case MQTT_DISCONNECT:
                {
                    /* Delete the subscriber and publisher tasks and go to the
                     * cleanup label as MQTT subscribe/publish has failed.
                     */
                    if (subscriber_task_handle != NULL)
                    {
                        vTaskDelete(subscriber_task_handle);
                    }
                    if (publisher_task_handle != NULL)
                    {
                        publisher_cleanup();
                        vTaskDelete(publisher_task_handle);
                    }
                    goto exit_cleanup;
                }
                default:
                    break;
            }
        }
    }

    /* Cleanup section for various operations. */
    exit_cleanup:
    cleanup();
    vTaskDelete( NULL );
}

/******************************************************************************
 * Function Name: wifi_connect
 ******************************************************************************
 * Summary:
 *  Function that initializes the Wi-Fi Connection Manager and then connects 
 *  to the Wi-Fi Access Point using the specified SSID and PASSWORD.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int : EXIT_SUCCESS on successful connection with a Wi-Fi Access Point,
 *        else EXIT_FAILURE.
 *
 ******************************************************************************/
static int wifi_connect(void)
{
    cy_rslt_t result;
    cy_wcm_connect_params_t connect_param;
    cy_wcm_ip_address_t ip_address;
    uint32_t retry_count;
    /* Configure the interface as a Wi-Fi STA (i.e. Client). */
    cy_wcm_config_t config = {.interface = CY_WCM_INTERFACE_TYPE_STA};

    /* Initialize the Wi-Fi Connection Manager and return if the operation fails. */
    result = cy_wcm_init(&config);
    CHECK_RESULT(result, WCM_INITIALIZED, "\nWi-Fi Connection Manager initialization failed!\n");
    printf("\nWi-Fi Connection Manager initialized.\n");

    /* Configure the connection parameters for the Wi-Fi interface. */
    memset(&connect_param, 0, sizeof(cy_wcm_connect_params_t));
    memcpy(connect_param.ap_credentials.SSID, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(connect_param.ap_credentials.password, WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
    connect_param.ap_credentials.security = WIFI_SECURITY;

    /* Connect to the Wi-Fi AP. */
    for (retry_count = 0; retry_count < MAX_WIFI_CONN_RETRIES; retry_count++)
    {
        printf("Connecting to Wi-Fi AP '%s'\n", connect_param.ap_credentials.SSID);
        result = cy_wcm_connect_ap(&connect_param, &ip_address);

        if (result == CY_RSLT_SUCCESS)
        {
            printf("Successfully connected to Wi-Fi network '%s'.\n",
                    connect_param.ap_credentials.SSID);

            /* Set the appropriate bit in the init_flag to denote successful
             * Wi-Fi connection, print the assigned IP address.
             */
            init_flag |= WIFI_CONNECTED;
            if (ip_address.version == CY_WCM_IP_VER_V4)
            {
                printf("IPv4 Address Assigned: %s\n\n", ip4addr_ntoa((const ip4_addr_t *) &ip_address.ip.v4));
            }
            else if (ip_address.version == CY_WCM_IP_VER_V6)
            {
                printf("IPv6 Address Assigned: %s\n\n", ip6addr_ntoa((const ip6_addr_t *) &ip_address.ip.v6));
            }
            return EXIT_SUCCESS;
        }

        printf("Connection to Wi-Fi network failed with error code 0x%0X. "
                "Retrying in %d ms...\n", (int)result, WIFI_CONN_RETRY_INTERVAL_MS);
        vTaskDelay(pdMS_TO_TICKS(WIFI_CONN_RETRY_INTERVAL_MS));
    }

    printf("Exceeded maximum Wi-Fi connection attempts\n\n");
    return EXIT_FAILURE;
}

/******************************************************************************
 * Function Name: mqtt_connect
 ******************************************************************************
 * Summary:
 *  Function that initializes the IoT SDK, network stack, and the MQTT client.
 *  Upon successful initialization the MQTT connect operation is performed.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int : EXIT_SUCCESS on successful completion of MQTT connection,
 *        else EXIT_FAILURE.
 *
 ******************************************************************************/
static int mqtt_connect(void)
{
    /* Variable to indicate status of various operations. */
    int result = EXIT_SUCCESS;

    /* MQTT client identifier string. */
    char mqtt_client_identifier[MQTT_CLIENT_IDENTIFIER_MAX_LEN] = {0};

    if (!IotSdk_Init())
    {
        result = EXIT_FAILURE;
    }
    CHECK_RESULT(result, IOT_SDK_INITIALIZED, "IoT SDK initialization failed!\n");
    printf("IoT SDK initialized successfully.\n");

    /* Initialize the Cypress Secure Sockets library. */
    result = IotNetworkSecureSockets_Init();
    CHECK_RESULT(result, NETWORK_STACK_INITIALIZED, "Network stack initialization failed!\n");
    printf("Network stack initialized successfully.\n");

    /* Initialize the MQTT library. */
    result = IotMqtt_Init();
    CHECK_RESULT(result, LIBS_INITIALIZED, "MQTT library initialization failed!\n\n");
    printf("MQTT library initialization successful.\n\n");

    /* Configure the user credentials for the AWS IoT Enhanced Custom 
     * Authentication.
     */
    if ((connectionInfo.awsIotMqttMode) && (strlen(MQTT_USERNAME) > 0))
    {
        connectionInfo.pUserName = MQTT_USERNAME;
        connectionInfo.pPassword = MQTT_PASSWORD;
        connectionInfo.userNameLength = sizeof(MQTT_USERNAME);
        connectionInfo.passwordLength = sizeof(MQTT_PASSWORD);
    }

    /* Configure the network interface and callback function for disconnection. */
    networkInfo.pNetworkInterface = IOT_NETWORK_INTERFACE_CY_SECURE_SOCKETS;
    networkInfo.disconnectCallback.function = mqtt_disconnect_callback;

    /* Every active MQTT connection must have a unique client identifier. */
    result = mqtt_get_unique_client_identifier(mqtt_client_identifier);
    CHECK_RESULT(result, 0, "Failed to generate unique client identifier for the MQTT client!\n");

    /* Set the client identifier buffer and length. */
    connectionInfo.pClientIdentifier = mqtt_client_identifier;
    connectionInfo.clientIdentifierLength = strlen(mqtt_client_identifier);

    printf("MQTT client '%.*s' connecting to MQTT broker '%s'...\n",
           connectionInfo.clientIdentifierLength,
           connectionInfo.pClientIdentifier,
           networkInfo.u.setup.pNetworkServerInfo->pHostName);

    /* Establish the MQTT connection. */
    result = IotMqtt_Connect(&networkInfo, &connectionInfo, MQTT_TIMEOUT_MS, &mqttConnection);
    CHECK_RESULT(result, CONNECTION_ESTABLISHED, "MQTT connection failed with error '%s'!\n\n", 
                 IotMqtt_strerror((IotMqttError_t) result));
    printf("MQTT connection successful.\n\n");
    return EXIT_SUCCESS;
}

/******************************************************************************
 * Function Name: mqtt_disconnect_callback
 ******************************************************************************
 * Summary:
 *  Callback invoked when the MQTT connection is disconnected. The function 
 *  informs the MQTT client task about the MQTT client disconnection using a
 *  message queue only if the disconnection was unexpected.
 *
 * Parameters:
 *  void *pCallbackContext : Information about expected disconnect reason (unused)
 *  IotMqttCallbackParam_t * pCallbackParam : Actual disconnection information 
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void mqtt_disconnect_callback(void *pCallbackContext,
                                     IotMqttCallbackParam_t *pCallbackParam)
{
    /* Structure that stores the data that will be sent to the MQTT client task
     * using a message queue.
     */
    mqtt_result_t mqtt_connection_status = MQTT_DISCONNECT;

    /* To avoid compiler warnings */
    (void)pCallbackContext;
    (void)pCallbackParam;

    /* Inform the MQTT client task about the disconnection except when the MQTT 
     * client has invoked the MQTT disconnect function.
     */
    if (pCallbackParam->u.disconnectReason != IOT_MQTT_DISCONNECT_CALLED)
    {
        printf("MQTT client disconnected unexpectedly!\n");
        
        xQueueOverwrite(mqtt_status_q, &mqtt_connection_status);
    }
}

/******************************************************************************
 * Function Name: mqtt_get_unique_client_identifier
 ******************************************************************************
 * Summary:
 *  Function that generates unique client identifier for the MQTT client by
 *  appending a timestamp to a common prefix (CLIENT_IDENTIFIER_PREFIX).
 *
 * Parameters:
 *  char *mqtt_client_identifier : Pointer to the string that stores the 
 *                                 generated unique identifier
 *
 * Return:
 *  int : EXIT_SUCCESS on successful generation of the client identifier,
 *        else EXIT_FAILURE
 *
 ******************************************************************************/
static int mqtt_get_unique_client_identifier(char *mqtt_client_identifier)
{
    int status = EXIT_SUCCESS;

    /* Check for errors from snprintf. */
    if (0 > snprintf(mqtt_client_identifier,
                     MQTT_CLIENT_IDENTIFIER_MAX_LEN,
                     MQTT_CLIENT_IDENTIFIER_PREFIX "%lu",
                     (long unsigned int)IotClock_GetTimeMs()))
    {
        status = EXIT_FAILURE;
    }

    return status;
}

/******************************************************************************
 * Function Name: cleanup
 ******************************************************************************
 * Summary:
 *  Function that invokes various deinit and cleanup functions for the 
 *  appropriate operations based on the init_flag.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void cleanup(void)
{
    /* Disconnect the MQTT connection if it was established. */
    if (init_flag & CONNECTION_ESTABLISHED)
    {
        printf("Disconnecting from the MQTT Server...\n");
        IotMqtt_Disconnect(mqttConnection, 0);
    }    
    /* Clean up libraries if they were initialized. */
    if (init_flag & LIBS_INITIALIZED)
    {
        IotMqtt_Cleanup();
    }
    /* Clean up the network stack if it was initialized. */
    if (init_flag & NETWORK_STACK_INITIALIZED)
    {
        IotNetworkSecureSockets_Cleanup();
    }
    /* Clean up the IoT SDK. */
    if (init_flag & IOT_SDK_INITIALIZED)
    {
        IotSdk_Cleanup();
    }
    /* Disconnect from Wi-Fi AP. */
    if (init_flag & WIFI_CONNECTED)
    {
        if (cy_wcm_disconnect_ap() == CY_RSLT_SUCCESS)
        {
            printf("Disconnected from the Wi-Fi AP!\n");
        }
    }
    /* De-initialize the Wi-Fi Connection Manager. */
    if (init_flag & WCM_INITIALIZED)
    {
        cy_wcm_deinit();
    }
}

/* [] END OF FILE */
