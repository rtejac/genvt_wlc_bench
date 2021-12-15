#!/bin/bash

image=add_low:latest
PORT_FILE=/tmp/port.txt

function start_container()
{
        if test -f $PORT_FILE; then
        	port=`cat $PORT_FILE`
	        new_port=`expr $port + 1`
        	echo $new_port > $PORT_FILE
	        port=new_port
	else
        	touch $PORT_FILE
		port=$2
        	echo $port > $PORT_FILE
	fi

       docker run -it --name weld_wl_`cat $PORT_FILE` --pid=host --env RTSP_IP=$1 --env RTSP_PORT=`cat $PORT_FILE` --env RTSP_STREAM=$3 --env MODEL_PRECISION=$4 --env INFERENCE_DEVICE=$5 --volume ~/share:/share --volume ~/.Xauthority:/root/.Xauthority --volume /tmp/.X11-unix/:/tmp/.X11-unix/ --env DISPLAY=${DISPLAY} --env XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR}  --env MQTT_IP=$6 --net=host --user root --rm $image /bin/bash
}

function stop_container()
{
   docker ps -q --filter ancestor=$image | xargs -r docker stop
}


if [[ "$1" == "stop" ]]
then
    echo "Stopping containers"
    stop_container $2
elif [ "$1" = "start" ]
then
    start_container  $2 $3 $4 $5 $6 $7 $8
else
    echo "Missing parameter. Ex: ./run_ADD_Weld_Porosity_proxy_wl_container.sh start <RTSP IP address > <RTSP start port number>  < RTSP stream name > <Model precision> < Target inference device> <MQTT_IP> or ./run_ADD_Weld_Porosity_proxy_wl_container.sh stop"
fi
