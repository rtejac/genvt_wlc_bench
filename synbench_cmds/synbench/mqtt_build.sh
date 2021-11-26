#!/bin/bash

# building MQTT
echo wlc123 | sudo -S git clone https://github.com/eclipse/paho.mqtt.c.git
cd ${HOME}/GenVT_Env/synbench/paho.mqtt.c
sudo make
sudo rm /usr/local/lib/libpaho-mqtt3*
sudo make install
cd ..
