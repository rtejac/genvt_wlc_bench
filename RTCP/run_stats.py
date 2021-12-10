import sys
import paho.mqtt.client as mqtt
from datetime import datetime
from time import sleep

client = mqtt.Client()
client.connect("localhost",1883,60)
client.loop_start()


def send_msg(client,topic,msg):

    #msg_format = '{\n'+ f"\t\"timestamp\" : \"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\",\n" + f"\t\"message\" : \"{msg}\"\n"+'}'
    
    msg_format = msg
    print(msg)
    client.publish(topic,msg_format)




with open(sys.argv[1],'r') as f:
    data = f.readlines()


for line in data:
    
    if 'Results' in line:
        
        key_index = data[data.index(line)]
        key = data[data.index(line)+2]
        if len(key.split()) > 1:
            for ind,i in enumerate(key_index.split()):
                if i == 'RTCP':
                    send_msg(client,'rtcp/RTCP_PID/kpi/rtcp_data',f"{i}_MAX :: {key.split()[ind]}")
                    sleep(0.5)


