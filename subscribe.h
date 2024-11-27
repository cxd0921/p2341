#ifndef _SUBSCRIBE_H
#define _SUBSCRIBE_H
//client init
#define MQTT_CLIENT_ID "spi_transfer_mqtt_sub"
#define MQTT_PORT "7887"
#define MQTT_HOSTNAME "127.0.0.1"
#define MQTT_KEEP_ALIVE 30
#define MQTT_TOPIC "UIST"

#define MQTT_UPDATE_LOG_DATA_ID 0x40000021 // 升级日志
#define MQTT_UPDATE_PROCESS_DATA_ID 0x40000018 // 升级进度
//data-id
#define MQTT_UPDATE_DATA_ID 0x40000017 // 升级状态
//data
#define MQTT_UPDATE_DATA_1 0x1         // 升级中
#define MQTT_UPDATE_DATA_2 0x2         // 升级结束
#define MQTT_UPDATE_DATA_3 0x3         // 升级失败
void parse_message(const struct mosquitto_message *msg, uint32_t *data_id, uint32_t *data);
int mqtt_sub_task();
#endif