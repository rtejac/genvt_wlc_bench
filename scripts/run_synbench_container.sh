#!/bin/bash

function start_container()
{
    if [ $# -ne 2 ]
    then
        echo "Missing parameter. Ex: ./run_synbench_container.sh <path to params.txt> or ./run_synbench_container.sh stop"
        exit
    fi

    param="$(cd "$(dirname "$2")" && pwd -P)/$(basename "$2")"
    file=$(basename "$2")
    echo "Param specified $param $file"

    docker run -t --privileged --device /dev/dri:/dev/dri --volume ~/.Xauthority:/root/.Xauthority \
    --volume $PWD/share:/share \
    --volume $param:/share/$file \
    --volume /tmp/.X11-unix:/tmp/.X11-unix --env DISPLAY=:0 \
    --env HTTP_PROXY=http://proxy-dmz.intel.com:911 --env HTTPS_PROXY=http://proxy-dmz.intel.com:912 \
    --env http_proxy=http://proxy-dmz.intel.com:911 --env https_proxy=http://proxy-dmz.intel.com:912 \
    --network host --entrypoint ./drivers.gpu.virtualization.synbench --rm $1:$tag /share/$file
   
}

function stop_container()
{
    docker ps -q --filter ancestor=$image:$tag | xargs -r docker stop
}

image=synbench
tag=2021-09-22

if [[ "$1" == *"txt"* ]]
then
    echo "Starting container"
    start_container $image $1
elif [ "$1" = "stop" ]
then
    echo "Stopping containers"
    stop_container $image
else
    echo "Missing parameter. Ex: ./run_synbench_container.sh <path to params.txt> or ./run_synbench_container.sh stop"
fi




