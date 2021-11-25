#ifndef __MSGBUS_H__
#define __MSGBUS_H__

#include "MQTTAsync.h"

int init_msg_bus(MQTTAsync* pClient);
void send_msg(MQTTAsync* pClient, const char* message_body, const char* message_type);
void cleanup_msg_bus(MQTTAsync* pClient);

#endif // __MSGBUS_H__