#if !MBED_CONF_AWS_CLIENT_SHADOW

#include "mbed.h"
#include "mbed-trace/mbed_trace.h"
#include "rtos/ThisThread.h"
#include "AWSClient/AWSClient.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"
#include "stm32l475e_iot01_magneto.h"
#include "stm32l475e_iot01_gyro.h"
#include "stm32l475e_iot01_accelero.h"

extern "C" {
#include "core_json.h"
}

#define TRACE_GROUP "Main"

static bool reply_received = false, looping = true, sensorsInit = false, sensorsUpdate = false;
const char devid[] = MBED_CONF_APP_AWS_CLIENT_IDENTIFIER;
const char fromNode[] = MBED_CONF_APP_FROMNODE;
const char toNode[] = MBED_CONF_APP_TONODE;

AWSClient &client = AWSClient::getInstance();

int subscribe(const char *topic)
{
    if(client.subscribe(topic,strlen(topic)) == MBED_SUCCESS) {
        tr_info("Subscribed to \"%.*s\" successfully",strlen(topic),topic);
        return true;
    } else {
        tr_error("AWS::subscribe() to \"%.*s\" FAILED",strlen(topic),topic);
        return !MBED_SUCCESS;
    }
}

int unsubscribe(const char *topic)
{
    if(client.unsubscribe(topic,strlen(topic)) == MBED_SUCCESS) {
        tr_info("Unsubscribed from \"%.*s\" successfully",strlen(topic),topic);
        return true;
    } else {
        tr_error("AWS::unsubscribe() from \"%.*s\" FAILED",strlen(topic),topic);
        return !MBED_SUCCESS;
    }
}

int sendMessage(char msgToSend[1024])
{
    char payload[1024], base_message[1024];
    sprintf(base_message, "%s", msgToSend);
    sprintf(payload, "{\n"
            "    \"sender\": \"%.*s\",\n"
            "    \"message\": %s\n"
            "}",
            strlen(devid),devid,base_message);
    tr_info("Publishing \"%s\" to topic \"%s\"", base_message, fromNode);
    int ret = client.publish(fromNode,strlen(fromNode),payload,strlen(payload));
    if (ret != MBED_SUCCESS) tr_error("AWSClient::publish() failed");
    rtos::ThisThread::sleep_for(1s);
    return ret;
}

int sendMessage()
{
    return sendMessage("\"Sample message\"");
}

void sensorsData()
{
    if(!sensorsInit) {
        printf("\nInitiating sensors...");
        BSP_TSENSOR_Init();
        BSP_HSENSOR_Init();
        BSP_PSENSOR_Init();
        BSP_MAGNETO_Init();
        BSP_GYRO_Init();
        BSP_ACCELERO_Init();
        sensorsInit=true;
        printf("successfully.\n");
        sensorsUpdate=true;
    }
    float gyrData[3] = {0};
    int16_t accData[3] = {0}, magData[3] = {0};
    BSP_GYRO_GetXYZ(gyrData);
    BSP_ACCELERO_AccGetXYZ(accData);
    BSP_MAGNETO_GetXYZ(magData);
    char tmp[1024];
    sprintf(tmp,"{\n"
            "   \"sensors\": {\n"
            "       \"Temperature\": \"%d\",\n"
            "       \"Humidity\": \"%d\",\n"
            "       \"Pressure\": \"%d\",\n"
            "       \"Gyro\": {\n"
            "           \"X\": \"%d\",\n"
            "           \"Y\": \"%d\",\n"
            "           \"Z\": \"%d\"\n"
            "       },\n"
            "       \"Accelerometer\": {\n"
            "           \"X\": \"%d\",\n"
            "           \"Y\": \"%d\",\n"
            "           \"Z\": \"%d\"\n"
            "       },\n"
            "       \"Magneto\": {\n"
            "           \"X\": \"%d\",\n"
            "           \"Y\": \"%d\",\n"
            "           \"Z\": \"%d\"\n"
            "       }\n"
            "   }\n"
            "}\n", (int)BSP_TSENSOR_ReadTemp(), (int)BSP_HSENSOR_ReadHumidity(), (int)BSP_PSENSOR_ReadPressure(), (int)gyrData[0], (int)gyrData[1], (int)gyrData[2], accData[0], accData[1], accData[2], magData[0], magData[1], magData[2]);
    sendMessage(tmp);
}

void extractSensors(const void *payload, size_t payload_length){
    char *json_value, *backup;
    size_t value_length, backuplen;
    
    printf("\nFrom AWS:");
    JSON_Search((char *)payload, payload_length, "Temperature", strlen("Temperature"), &json_value, &value_length);
    printf("\nTemperature:\"%.*s\",",value_length,json_value);
    JSON_Search((char *)payload, payload_length, "Humidity", strlen("Humidity"), &json_value, &value_length);
    printf("\nHumidity:\"%.*s\",",value_length,json_value);
    JSON_Search((char *)payload, payload_length, "Pressure", strlen("Pressure"), &json_value, &value_length);
    printf("\nPressure:\"%.*s\",",value_length,json_value);

    JSON_Search((char *)payload, payload_length, "Gyro", strlen("Gyro"), &json_value, &value_length);
    printf("\nGyro: { ");
    backup=json_value; backuplen=value_length;
        JSON_Search(json_value, value_length, "X", strlen("X"), &json_value, &value_length);
        printf("X:\"%.*s\", ",value_length,json_value);
    
    json_value=backup; value_length=backuplen;
        JSON_Search(json_value, value_length, "Y", strlen("Y"), &json_value, &value_length);
        printf("Y:\"%.*s\", ",value_length,json_value);

    json_value=backup; value_length=backuplen;
        JSON_Search(json_value, value_length, "Z", strlen("Z"), &json_value, &value_length);
        printf("Z:\"%.*s\" },",value_length,json_value);    
    
    JSON_Search((char *)payload, payload_length, "Accelerometer", strlen("Accelerometer"), &json_value, &value_length);
    printf("\nAccelerometer: { ");
        backup=json_value; backuplen=value_length;
        JSON_Search(json_value, value_length, "X", strlen("X"), &json_value, &value_length);
        printf("X:\"%.*s\", ",value_length,json_value);
    
    json_value=backup; value_length=backuplen;
        JSON_Search(json_value, value_length, "Y", strlen("Y"), &json_value, &value_length);
        printf("Y:\"%.*s\", ",value_length,json_value);

    json_value=backup; value_length=backuplen;
        JSON_Search(json_value, value_length, "Z", strlen("Z"), &json_value, &value_length);
        printf("Z:\"%.*s\" },",value_length,json_value);
    
    JSON_Search((char *)payload, payload_length, "Magneto", strlen("Magneto"), &json_value, &value_length);
    printf("\nMagneto: { ");
        backup=json_value; backuplen=value_length;
        JSON_Search(json_value, value_length, "X", strlen("X"), &json_value, &value_length);
        printf("X:\"%.*s\", ",value_length,json_value);
    
    json_value=backup; value_length=backuplen;
        JSON_Search(json_value, value_length, "Y", strlen("Y"), &json_value, &value_length);
        printf("Y:\"%.*s\", ",value_length,json_value);

    json_value=backup; value_length=backuplen;
        JSON_Search(json_value, value_length, "Z", strlen("Z"), &json_value, &value_length);
        printf("Z:\"%.*s\" }\n",value_length,json_value);
}

// Callback when a MQTT message has been added to the topic
void on_message_callback(
    const char *topic,
    uint16_t topic_length,
    const void *payload,
    size_t payload_length)
{

    char *json_value;
    size_t value_length;
    if (strncmp(topic,fromNode,strlen(fromNode))==0){
        auto ret = JSON_Search((char *)payload, payload_length, "sender", strlen("sender"), &json_value, &value_length);
        if (ret == JSONSuccess){
            tr_info("Sender:\"%.*s\" in \"%.*s\"", value_length, json_value, topic_length, topic);
            if (strncmp(json_value, devid, strlen(devid)) == 0)
                tr_info("Message sent successfully to \"%.*s\"",topic_length,topic);
            else {
                ret = JSON_Search((char *)payload, payload_length, "message", strlen("message"), &json_value, &value_length);
                if(ret == JSONSuccess)
                    tr_info("\"%.*s\"",value_length,json_value);
                else
                    tr_error("Failed to extract message from the payload: \"%.*s\"", payload_length, (const char *) payload);
            }
        }else
            tr_error("Failed to extract sender from the payload: \"%.*s\"", payload_length, (const char *) payload);        
    }else if (strncmp(topic,toNode,strlen(toNode))==0){
        auto ret = JSON_Search((char *)payload, payload_length, "sender", strlen("sender"), &json_value, &value_length);
        if (ret == JSONSuccess){
            tr_info("Received message from \"%.*s\" on \"%.*s\"",value_length,json_value,topic_length,topic);
            if(strncmp(json_value,"AWS",strlen("AWS"))==0){
                //tr_info("***from aws stuff here***");
                ret = JSON_Search((char *)payload, payload_length, "message", strlen("message"), &json_value, &value_length);
                if (ret == JSONSuccess){
                    if(strncmp(json_value,"updatesensors",strlen("updatesensors"))==0)
                        sensorsData();
                }else{
                    ret = JSON_Search((char *)payload, payload_length, "sensorsdata", strlen("sensorsdata"), &json_value, &value_length);
                    if (ret == JSONSuccess)
                        extractSensors(json_value, value_length);
                    else
                        tr_error("Failed to extract message from the payload: \"%.*s\"", payload_length, (const char *) payload);
                }
            }
        } else
            tr_error("Failed to extract payload: \"%.*s\"", payload_length, (const char *) payload);        
    } else printf("UNK topic: \"%.*s\"\n", topic_length, topic);
}

void demo()
{
    subscribe(toNode);
    if(subscribe(fromNode)) {
        while(looping) {
            while(!reply_received) sleep();
            rtos::ThisThread::sleep_for(1s);
        }
    } else {
        tr_error("AWSClient::subscribe() failed");
        return;
    }
}

#endif // !MBED_CONF_AWS_CLIENT_SHADOW