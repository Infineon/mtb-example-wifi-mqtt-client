/******************************************************************************
* File Name:   subscriber_task.c
*
* Description: This file contains the task that initializes the user LED GPIO,
*              subscribes to the topic 'MQTT_TOPIC', and actuates the user LED
*              based on the notifications received from the MQTT subscriber
*              callback.
*
* Related Document: See README.md
*
*******************************************************************************
* (c) 2020-2021, Cypress Semiconductor Corporation. All rights reserved.
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
#include "string.h"
#include "FreeRTOS.h"

/* Task header files */
#include "subscriber_task.h"
#include "mqtt_task.h"

/* Configuration file for MQTT client */
#include "mqtt_client_config.h"

/* Middleware libraries */
#include "cy_retarget_io.h"
#include "iot_mqtt.h"

/******************************************************************************
* Macros
******************************************************************************/
/* The number of MQTT topics to be subscribed to. */
#define SUBSCRIPTION_COUNT          (1)

/******************************************************************************
* Function Prototypes
*******************************************************************************/
static void mqtt_subscription_callback(void *pCallbackContext,
                                       IotMqttCallbackParam_t *pPublishInfo);

/******************************************************************************
* Global Variables
*******************************************************************************/
/* Task handle for this task. */
TaskHandle_t subscriber_task_handle;

/* Variable to denote the current state of the user LED that is also used by 
 * the publisher task.
 */
uint32_t current_device_state = DEVICE_OFF_STATE;

/* Configure the subscription information structure. */
IotMqttSubscription_t subscribeInfo =
{
    .qos = (IotMqttQos_t) MQTT_MESSAGES_QOS,
    .pTopicFilter = MQTT_TOPIC,
    .topicFilterLength = (sizeof(MQTT_TOPIC) - 1),
    /* Configure the callback function to handle incoming MQTT messages */
    .callback.function = mqtt_subscription_callback
};

/******************************************************************************
 * Function Name: subscriber_task
 ******************************************************************************
 * Summary:
 *  Task that sets up the user LED GPIO, subscribes to topic - 'MQTT_TOPIC',
 *  and controls the user LED based on the received task notification.
 *
 * Parameters:
 *  void *pvParameters : Task parameter defined during task creation (unused)
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void subscriber_task(void *pvParameters)
{
    /* Status variable */
    int result = EXIT_SUCCESS;

    /* Variable to denote received LED state. */
    uint32_t received_led_state;

    /* Status of MQTT subscribe operation that will be communicated to MQTT 
     * client task using a message queue in case of failure in subscription.
     */
    mqtt_result_t mqtt_subscribe_status = MQTT_SUBSCRIBE_FAILURE;

    /* To avoid compiler warnings */
    (void)pvParameters;

    /* Initialize the User LED. */
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_PULLUP,
                    CYBSP_LED_STATE_OFF);

    /* Subscribe with the configured parameters. */
    result = IotMqtt_SubscribeSync(mqttConnection,
                                   &subscribeInfo,
                                   SUBSCRIPTION_COUNT,
                                   0,
                                   MQTT_TIMEOUT_MS);
    if (result != EXIT_SUCCESS)
    {
        /* Notify the MQTT client task about the subscription failure and  
         * suspend the task for it to be killed by the MQTT client task later.
         */
        printf("MQTT Subscribe failed with error '%s'.\n\n",
               IotMqtt_strerror((IotMqttError_t) result));
        xQueueOverwrite(mqtt_status_q, &mqtt_subscribe_status);
        vTaskSuspend( NULL );
    }

    printf("MQTT client subscribed to the topic '%.*s' successfully.\n\n", 
           subscribeInfo.topicFilterLength, subscribeInfo.pTopicFilter);

    while (true)
    {
        /* Block till a notification is received from the subscriber callback. */
        xTaskNotifyWait(0, 0, &received_led_state, portMAX_DELAY);
        /* Update the LED state as per received notification. */
        cyhal_gpio_write(CYBSP_USER_LED, received_led_state);
        /* Update the current device state extern variable. */
        current_device_state = received_led_state;
    }
}

/******************************************************************************
 * Function Name: mqtt_subscription_callback
 ******************************************************************************
 * Summary:
 *  Callback to handle incoming MQTT messages. This callback prints the 
 *  contents of an incoming message and notifies the subscriber task with the  
 *  LED state as per the received message.
 *
 * Parameters:
 *  void *pCallbackContext : Parameter defined during MQTT Subscribe operation
 *                           using the IotMqttCallbackInfo_t.pCallbackContext
 *                           member (unused)
 *  IotMqttCallbackParam_t * pPublishInfo : Information about the incoming 
 *                                          MQTT PUBLISH message passed by
 *                                          the MQTT library.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void mqtt_subscription_callback(void *pCallbackContext,
                                       IotMqttCallbackParam_t *pPublishInfo)
{
    /* Received MQTT message */
    const char *pPayload = pPublishInfo->u.message.info.pPayload;
    /* LED state that should be sent to LED task depending on received message. */
    uint32_t subscribe_led_state = DEVICE_OFF_STATE;

    /* To avoid compiler warnings */
    (void) pCallbackContext;

    /* Print information about the incoming PUBLISH message. */
    printf("Incoming MQTT message received:\n"
           "Subscription topic filter: %.*s\n"
           "Published topic name: %.*s\n"
           "Published QoS: %d\n"
           "Published payload: %.*s\n\n",
           pPublishInfo->u.message.topicFilterLength,
           pPublishInfo->u.message.pTopicFilter,
           pPublishInfo->u.message.info.topicNameLength,
           pPublishInfo->u.message.info.pTopicName,
           pPublishInfo->u.message.info.qos,
           pPublishInfo->u.message.info.payloadLength,
           pPayload);

    /* Assign the LED state depending on the received MQTT message. */
    if ((strlen(MQTT_DEVICE_ON_MESSAGE) == pPublishInfo->u.message.info.payloadLength) &&
        (strncmp(MQTT_DEVICE_ON_MESSAGE, pPayload, pPublishInfo->u.message.info.payloadLength) == 0))
    {
        subscribe_led_state = DEVICE_ON_STATE;
    }
    else if ((strlen(MQTT_DEVICE_OFF_MESSAGE) == pPublishInfo->u.message.info.payloadLength) &&
             (strncmp(MQTT_DEVICE_OFF_MESSAGE, pPayload, pPublishInfo->u.message.info.payloadLength) == 0))
    {
        subscribe_led_state = DEVICE_OFF_STATE;
    }
    else
    {
        printf("Received MQTT message not in valid format!\n");
        return;
    }

    /* Notify the subscriber task about the received LED control message. */
    xTaskNotify(subscriber_task_handle, subscribe_led_state, eSetValueWithoutOverwrite);
}

/******************************************************************************
 * Function Name: mqtt_unsubscribe
 ******************************************************************************
 * Summary:
 *  Function that unsubscribes from the topic specified by the macro 
 *  'MQTT_TOPIC'. This operation is called during cleanup by the MQTT client 
 *  task.
 *
 * Parameters:
 *  void 
 *
 * Return:
 *  void 
 *
 ******************************************************************************/
void mqtt_unsubscribe(void)
{
    IotMqttError_t result = IotMqtt_UnsubscribeSync(mqttConnection,
                                                    &subscribeInfo,
                                                    SUBSCRIPTION_COUNT,
                                                    0,
                                                    MQTT_TIMEOUT_MS);
    if (result != IOT_MQTT_SUCCESS)
    {
        printf("MQTT Unsubscribe operation failed with error '%s'!\n",
               IotMqtt_strerror(result));
    }
}

/* [] END OF FILE */
