#!/bin/bash
if [ -z $1 ];then
    echo "syntex:./init.sh wl_name"
    echo $1
    exit
fi
python3 genvt_scp.py $1 '/home/wlc/GenVT_Env/'
#o1=$?
python3 genvt_scp.py start_wl.sh '/home/wlc/start_wl.sh'
python3 genvt_scp.py init_wl.sh '/home/wlc/init_wl.sh'
#o2=$?
#python3 genvt_scp.py $3 '/home/wlc/'
python3 genvt_ssh.py 'chmod +x start_wl.sh'
#o3=$?
python3 genvt_ssh.py 'chmod +x init_wl.sh'
python3 genvt_ssh.py './init_wl.sh' 'proxy_init_log.log'

#echo 'Executed all the commands with exit codes ' $o1 $o2 $o3
#python3 genvt_ssh.py 'hostname -I'
