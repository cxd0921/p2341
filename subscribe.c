#include "mosquitto.h" 
#include <stdio.h>    
#include <stdlib.h>  
#include <string.h>   
#include <unistd.h>   
#include "subscribe.h"
#include "Update_Process.h"
#include "Update_Driver.h"
#include <pthread.h>

pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

void on_connect(struct mosquitto *mosq, void *obj, int reason_code) {
    int rc;
    akst_debug("on_connect success: %s\n", mosquitto_connack_string(reason_code));
    if (reason_code != 0) {
        mosquitto_disconnect(mosq);
    }else {
        rc = mosquitto_subscribe(mosq, NULL, MQTT_TOPIC, 1);
        if (rc!= MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
            mosquitto_disconnect(mosq);
        }
    }
}

void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos) {
    int i;
    bool have_subscription = false;
    for (i = 0; i < qos_count; i++) {
        akst_debug("on_subscribe: %d: granted qos = %d\n", i, granted_qos[i]);
        if (granted_qos[i] <= 2) {
            have_subscription = true;
        }
    }
    if (!have_subscription) {
        fprintf(stderr, "Error: All subscriptions rejected.\n");
        mosquitto_disconnect(mosq);
    }
}

void parse_message(const struct mosquitto_message *msg, uint32_t *data_id, uint32_t *data) {
    if (msg->payloadlen >= (sizeof(uint32_t) + sizeof(uint32_t))) {
        memcpy(data_id, msg->payload, sizeof(uint32_t));
        memcpy(data, (char *)msg->payload + sizeof(uint32_t), sizeof(uint32_t));
    }
}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    uint32_t received_data_id;
    uint32_t received_data;
    parse_message(msg, &received_data_id, &received_data);
    //akst_debug("\n-----Received MQTT Message: data_id = %u, data = %u-----\n", received_data_id, received_data);
    if (received_data_id == MQTT_UPDATE_DATA_ID) {
        pthread_mutex_lock(&status_mutex);
        switch (received_data) {
        case MQTT_UPDATE_DATA_1:
            akst_debug("\nUpdating Form MQTT: data_id = %u, data = %u\n\n", received_data_id, received_data);
            break;
        case MQTT_UPDATE_DATA_2:
            akst_debug("\nUpdate Success Form MQTT: data_id = %u, data = %u\n\n", received_data_id, received_data);
            write_0x18_1_byte(UPDATE_SUCCESS);
            break;
        case MQTT_UPDATE_DATA_3:
            akst_debug("\nUpdate Failed Form MQTT: data_id = %u, data = %u\n\n", received_data_id, received_data);
            write_0x18_1_byte(UPDATE_FAILED);
            break;
        default:
            akst_debug("-------MQTT-------Invalid Value: %u\n\n", received_data);
            break;
        }
        pthread_mutex_unlock(&status_mutex);
    }
    
}

int mqtt_sub_task() {
    struct mosquitto *mosq;
    int rc;
    mosquitto_lib_init();
    mosq = mosquitto_new(MQTT_CLIENT_ID, true, NULL);
    if (mosq == NULL) {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_subscribe_callback_set(mosq, on_subscribe);
    mosquitto_message_callback_set(mosq, on_message);

    rc = mosquitto_connect(mosq, MQTT_HOSTNAME,atoi(MQTT_PORT), MQTT_KEEP_ALIVE);;
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        fprintf(stderr, "connect failed: %s\n", mosquitto_strerror(rc));
        return 1;
    }

    mosquitto_loop_forever(mosq, -1, 1);
    mosquitto_lib_cleanup();
    return 0;
}