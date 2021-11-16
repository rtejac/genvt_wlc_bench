#!/bin/bash

# running synbench
cd ${HOME}/GenVT_Env/synbench/synbench/
export DESTDIR=$PWD
export DISPLAY=:1
export MQTT_IP=10.99.114.167
./synbench params/intel_indu_hmi_low_profile.txt
