#include "main.h"
#include "kpi.h"
#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "msgbus.h"

#define                      SYNBENCH_STATUS_FMT            "synbench/%d/kpi/status"


static char                  MQTT_ADDRESS[50]               = {0};
static const char*           MQTT_PORT_SUFFIX               = ":1883";




//static const char*           MQTT_ADDRESS                   = "tcp://localhost:1883";
static const char*           MQTT_PAYLOAD_FMT               = "{\n\t\"timestamp\": \"%s\",\n\t\"message\": \"%s\"\n}";
static const int             MQTT_QOS                       = 1;
static const long            MQTT_TIMEOUT                   = 10000L;
static MQTTAsync_willOptions gWill_opts                     = MQTTAsync_willOptions_initializer;
static char                  gTopicStatus[32]               = {0};
static char                  g_mqtt_client_name_unique[256] = {0};

extern char*                __progname;


int init_msg_bus(MQTTAsync* pClient) {
    int                         ret                         = MQTTASYNC_SUCCESS;
    int                         pid                         = getpid();
    MQTTAsync_connectOptions    conn_opts                   = MQTTAsync_connectOptions_initializer;
    const char*                 mqtt_client_name_unique_fmt = "%s.%d";
    bool                        bConnected                  = false;
    
    snprintf(g_mqtt_client_name_unique, sizeof(g_mqtt_client_name_unique), mqtt_client_name_unique_fmt, __progname, pid);
    snprintf(gTopicStatus, sizeof(gTopicStatus), SYNBENCH_STATUS_FMT, pid);
    

const char* mqtt_ip = std::getenv("MQTT_IP"); 

std::strcpy( MQTT_ADDRESS , "tcp://" );
if (std::strlen(mqtt_ip) > 0 )
{
        std::strcat(MQTT_ADDRESS, mqtt_ip);
}
else
{
        std::strcat(MQTT_ADDRESS, "localhost");
}

std::strcat(MQTT_ADDRESS, MQTT_PORT_SUFFIX);


printf("MQTT_ADDRESS is %s/n",MQTT_ADDRESS);
ret = MQTTAsync_create(pClient, MQTT_ADDRESS, g_mqtt_client_name_unique, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (MQTTASYNC_SUCCESS == ret) {
        gWill_opts.message = "lwt";
        gWill_opts.topicName = gTopicStatus;

        conn_opts.keepAliveInterval = 20;
        conn_opts.cleansession = 1;
        conn_opts.will = &gWill_opts;

        ret = MQTTAsync_connect(*pClient, &conn_opts);

        printf("Waiting for successful MQTT connection or timeout...\n");
        unsigned int attemptCount = 0;
        do {
            usleep(250000);
            bConnected = MQTTAsync_isConnected(*pClient);
            attemptCount++;
        } while((!bConnected) && (attemptCount <= 12));
    }

    if (!bConnected) {
        ret = -1;
    }

    return ret;
}

void send_msg(MQTTAsync* pClient, const char* message_body, const char* message_type) {
    MQTTAsync_message           pubmsg              = MQTTAsync_message_initializer;
    MQTTAsync_responseOptions   opts                = MQTTAsync_responseOptions_initializer;
    struct timeval              tv;
    struct tm*                  tm;
    time_t                      seconds;
    char                        tm_buf[32];
    char                        msg_buffer[256];

    if (pClient) {
        gettimeofday(&tv, NULL);
        seconds = tv.tv_sec;
        tm = localtime(&seconds);
        strftime(tm_buf, sizeof(tm_buf), "%Y-%m-%d %H:%M:%S", tm);
        //snprintf(tm_buf, sizeof(tm_buf), "%s.%06ld", tm_buf_temp, tv.tv_usec);

        int msg_size = snprintf(msg_buffer, sizeof(msg_buffer), MQTT_PAYLOAD_FMT, tm_buf, message_body);
        if ((msg_size > 0) && (msg_size < (int)sizeof(msg_buffer))) {
            pubmsg.payload = (void*)msg_buffer;
            pubmsg.payloadlen = msg_size;
            pubmsg.qos = MQTT_QOS;
            pubmsg.retained = 0;

            opts.context = *pClient;

            MQTTAsync_sendMessage(*pClient, message_type, &pubmsg, &opts);
        } else {
            printf("ERROR: mqtt message buffer too small\n");
            exit_synbench(-1);
        }
    }
}

void cleanup_msg_bus(MQTTAsync* pClient) {
    if (pClient) {
        MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
        opts.context = *pClient;
        MQTTAsync_disconnect(*pClient, &opts);
        MQTTAsync_destroy(pClient);
    }
}
