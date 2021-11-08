#!/bin/bash


echo wlc123 | sudo -S su
cd ~/GenVT_Env/synbench/synbench
make
export DISPLAY=:0
export DESTDIR=$PWD
./synbench params/intel_indu_hmi_low_profile.txt
