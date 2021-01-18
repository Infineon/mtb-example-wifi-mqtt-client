/******************************************************************************
* File Name:   mqtt_client_config.c
*
* Description: This file contains the configuration structures used by 
*              the MQTT client for MQTT connect operation.
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

#include <stdio.h>
#include "mqtt_client_config.h"

/******************************************************************************
* Global Variables
*******************************************************************************/
/* Configure the MQTT Broker/Server details. */
struct IotNetworkServerInfo networkServerInfo =
{
    .pHostName = MQTT_BROKER_ADDRESS,
    .port = MQTT_PORT
};

#if (MQTT_SECURE_CONNECTION)
/* Configure the MQTT client credentials in case of a secure connection. */
struct IotNetworkCredentials credentials =
{
    /* Configure the client certificate. */
    .pClientCert = (const char *)CLIENT_CERTIFICATE,
    .clientCertSize = sizeof(CLIENT_CERTIFICATE),
    /* Configure the client private key. */
    .pPrivateKey = (const char *)CLIENT_PRIVATE_KEY,
    .privateKeySize = sizeof(CLIENT_PRIVATE_KEY),
    /* Configure the Root CA certificate of the MQTT Broker/Server. */
    .pRootCa = (const char *)ROOT_CA_CERTIFICATE,
    .rootCaSize = sizeof(ROOT_CA_CERTIFICATE),
    /* Application Layer Protocol Negotiation (ALPN) is used to implement 
     * MQTT with TLS Client Authentication on Port 443 from client devices.
     */
    #if (MQTT_PORT == 443)
    .pAlpnProtos = MQTT_ALPN_PROTOCOL_NAME
    #else
    .pAlpnProtos = NULL
    #endif 
};

/* Structure with the network interface details. */
IotMqttNetworkInfo_t networkInfo =
{
    .createNetworkConnection = true,
    .u.setup.pNetworkCredentialInfo = &credentials,
    .u.setup.pNetworkServerInfo = &networkServerInfo
};

#else
/* Structure with the network interface details. */
IotMqttNetworkInfo_t networkInfo =
{
    .createNetworkConnection = true,
    /* Set the credentials to NULL for a non-secure connection. */
    .u.setup.pNetworkCredentialInfo = NULL,
    .u.setup.pNetworkServerInfo = &networkServerInfo
};
#endif /* #if (MQTT_SECURE_CONNECTION) */

#if ENABLE_LWT_MESSAGE
/* Last Will and Testament (LWT) message structure. The MQTT broker will
 * publish the LWT message if this client disconnects unexpectedly.
 */
IotMqttPublishInfo_t willInfo =
{
    .qos = IOT_MQTT_QOS_0,
    .pTopicName = MQTT_WILL_TOPIC_NAME,
    .topicNameLength = (uint16_t)(sizeof(MQTT_WILL_TOPIC_NAME) - 1),
    .pPayload = MQTT_WILL_MESSAGE,
    .payloadLength = (size_t)(sizeof(MQTT_WILL_MESSAGE) - 1)
};
#endif /* ENABLE_LWT_MESSAGE */

/* MQTT connection information structure. */
IotMqttConnectInfo_t connectionInfo =
{
    .cleanSession = true,
    .awsIotMqttMode = (AWS_IOT_MQTT_MODE == 1),
    .keepAliveSeconds = MQTT_KEEP_ALIVE_SECONDS,
    #if ENABLE_LWT_MESSAGE
    .pWillInfo = &willInfo,
    #else
    .pWillInfo = NULL,
    #endif /* ENABLE_LWT_MESSAGE */
    .pUserName = NULL,
    .pPassword = NULL,
    .userNameLength = 0,
    .passwordLength = 0
};

/* Check for a valid QoS setting. 
 * The MQTT library currently supports only QoS 0 and QoS 1.
 */
#if ((MQTT_MESSAGES_QOS != 0) && (MQTT_MESSAGES_QOS != 1))
    #error "Invalid QoS setting! MQTT_MESSAGES_QOS must be either 0 or 1."
#endif

/* Check if the macros are correctly configured for AWS IoT Broker. */
#if ((MQTT_SECURE_CONNECTION != 1) && (AWS_IOT_MQTT_MODE == 1))
    #error "AWS IoT does not support unsecured connections!"
#endif

/* [] END OF FILE */
