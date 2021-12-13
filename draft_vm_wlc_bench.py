#!/usr/bin/env python3
#
# INTEL CONFIDENTIAL
#
# Copyright 2021 (c) Intel Corporation.
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them  is governed by the  express license under which  they were
# provided to you ("License"). Unless the License provides otherwise, you  may
# not  use,  modify,  copy, publish,  distribute,  disclose  or transmit  this
# software or the related documents without Intel"s prior written permission.
#
# This software and the related documents are provided as is, with no  express
# or implied  warranties, other  than those  that are  expressly stated in the
# License.
#
# ----------------------------------------------------------------------------

import yaml
import datetime
import logging
import subprocess
import os
import shutil
import sys
import time
from getpass import getpass
from pathlib import Path

from datetime import datetime
import coloredlogs

from broker import Broker
from system_metrics import SystemMetrics
from wl_launcher import WlLauncher
from yml_parser import YAMLParser

from VM_Support.vm_support import VM, validate_yaml
import paho.mqtt.client as mqtt


def main():
    """
    Main function
    1. parse cmd line
    2. call yaml parser
    3. extract values
    4. start broker
    5. initiate system_metrics, runner
    6. cleanup
    7. report density/max fps number
    :return: wlc density/max fps value
    """
    # Check arguments
    if len(sys.argv) != 2:
        sys.exit("Usage: ./wlc_bench.py <config.yaml>")

    # no longer need to specify breakpoint_serial, only takes 1 arg - yml file
    yml_file = sys.argv[1]

    # default mode, none other currently defined
    wlc_bench_mode = "breakpoint_serial"

    # parse YML file
    parser = YAMLParser(yml_file)
    parsed_file = parser.parse()

    # get proxy, measured wl params
    settling_time, log_level = get_yml_values(wlc_bench_mode, parsed_file, parser)

    # setup logging
    setup_logging(log_level)

    mode = parser.get(wlc_bench_mode, parsed_file)

    # get list of env variables
    env_var_list = parser.get("env_vars", mode)
    for var in env_var_list:
        temp = var.split("=")
        logging.info(f"Setting env variable: {var}")
        os.environ[temp[0]] = temp[1]
    
    #Teja edit
    
    # get mqtt values
    mqtt_host, mqtt_port = get_mqtt_yml_values("mqtt", parsed_file, parser)

    broker = Broker(mqtt_host, mqtt_port)
    broker.start()
    
    
    logging.info(f'PID of this vm_wlc_bench instance is {os.getpid()}')

    with open('sample_dict.txt','w') as f:
        f.write('=============LOGGING MESSAGES==============\n')
    #Gettitng the VM related wl details in a list
    
    vmExist = False
    for k,v in mode.items():
        if 'vm' in k:
            vmExist = True
            break
    
    if vmExist:
        validate_yaml(parser,mode)
    
    vm_list = []
    #found_measured = False
    only_measured = {}

    rtcp = None
        
    for k,v in mode.items():
        if 'vm' in k:
            
            proxy_info = {}
            measured_info = {}
            current_vm = parser.get(k, mode)
            proxy_info[k] = current_vm['proxy_wl']
            proxy_info[k]['type'] = 'proxy'
            
            global rtcp_flag
            isRTCP = False
            if current_vm['RTCP']:
                rtcp_flag += 1
                isRTCP = True

            try:
                measured_info[k] = current_vm['measured_wl']
                only_measured[k] = current_vm['measured_wl']
            except:
                measured_info[k] = None

            vm_index = int(k.split('_')[1]) #to get 0,1 from vm_0,vm_1 and use them for creating VM index
            
            #Instansating VM object
            vm_object = VM(current_vm['vm_name'],current_vm['os_name'],current_vm['os_image'],vm_index,proxy_info[k],measured_info[k],current_vm['gpu_passthrough'],current_vm['ram'],current_vm['cpu'],isRTCP)
            
            #Creating VM
            vm_object.create_vm()
            vm_list.append(vm_object)
    
    workloads = {}
    
    for vm_object in vm_list:

        vm_object.proxy_init_exec()
        vm_object.measured_init_exec()
        
        if vm_object.isRTCP:
            prefix = 'RTVM '
        else:
            prefix = 'Non RTVM '
        workloads[prefix+vm_object.vm_name] = setup_workloads(vm_object.proxy, vm_object.measured)
    
    measured_wkld_vm = []
    no_measured_wkld_vm = []
    rtvm_measured_wkld_vm = []
    rtvm_no_measured_wkld_vm = []
    for k,v in mode.items():
        if 'Service_OS' in k:
            wklds = parser.get(k,mode)
            proxy = wklds["proxy_wl"]
            proxy["type"] = "proxy"
            try:
                measured = wklds["measured_wl"]
                only_measured[k] = wklds['measured_wl']
            except:
                measured = None

            workloads['Service_OS'] = setup_workloads(proxy, measured)
    
    print(workloads)
    exit()
    for cat,workload in workloads.items():
        if cat == 'RTVM' and workload[1]['isExist']:
            rtvm_measured_wkld_vm.append(workload)
        if cat == 'Non RTVM' and  workload[1]['isExist']:
            measured_wkld_vm.append(workload)
        
        if cat == 'RTVM' and not workload[1]['isExist']:
            rtvm_no_measured_wkld_vm.append(workload)
        if cat == 'RTVM' and not workload[1]['isExist']:
            no_measured_wkld_vm.append(workload)
        
        #if workload[1]['isExist']:
        #    measured_wkld_vm.append(workload)
        #else:
        #    no_measured_wkld_vm.append(workload)
    ''' 
    if rtcp_flag:
        
        logging.info(f'{rtcp_flag} VMs are having RTCP wkld running, need {rtcp_flag} "completed" messages at in total')
            
        def on_message(client, userdata, msg):
            global rtcp_flag
            if 'completed' in msg.payload.decode('utf-8'):
                logging.info(f'Completed message received from 1 VM, {rtcp_flag - 1} pending')
                rtcp_flag -= 1
            elif 'RTCP' in msg.payload.decode('utf-8'):
                logging.info(msg.payload.decode('utf-8'))
        
        client = mqtt.Client()
        client.connect("localhost",1883,60)
        client.subscribe('rtcp/+/kpi/+')
        client.on_message = on_message
        logging.info(f"Waiting for the 'completed' message from VMs having RTCP workloads")
       client.loop_start()
    '''
    #Done
    

    runner = system_metrics = None

    try:
        metrics = parser.get("system_metrics", parsed_file)
        is_metrics_capture = parser.get("measure", metrics)

        system_metrics = SystemMetrics(is_metrics_capture)

        # metrics=True, start tools
        if is_metrics_capture:

            # get sudo password, avoid asking each time in for loop
            logging.info("Please enter sudo password which will be used for running metrics tools "
                         "requiring sudo access")
            system_metrics.sudo_password = getpass()

            tools = parser.get("tools", metrics)

            for tool in tools:
                name = tool["name"]
                loc = tool["loc"] if "loc" in tool else None

                # nothing to start for pdu, skip start step only for pdu
                if "pdu" in name:
                    if tool["use_pdu"]:
                        system_metrics.tool_bank.append(tool)
                    else:
                        logging.warning(f"use_pdu False. Skipping pdu metrics")
                    # no command to start for pdu, skip iteration
                    continue

                # start all tools
                if system_metrics.check_tool_exists(name, loc=loc):
                    system_metrics.tool_bank.append(tool)
                    if "socwatch" in name:
                        continue
                    system_metrics.start(tool)

            # first pass, no workloads, system idle measurements
            system_metrics.collect_store()
        
        #Teja edit
        for wkld in no_measured_wkld_vm:   #for i,wkld in enumerate(workloads):
            # start workload launcher
            runner = WlLauncher(wkld, settling_time, system_metrics, broker)
            runner.run()
        
        for wkld in measured_wkld_vm:   #for i,wkld in enumerate(workloads):
            # start workload launcher
            runner = WlLauncher(wkld, settling_time, system_metrics, broker)
            runner.run()
        
        #logging.info(f'{rtcp_flag} completed messages are expected to come from RTCP workloads') 
        #while rtcp_flag > 0:
        #    if rtcp_flag == 0:
        #        logging.info('Stopping all the workloadds in all the VMs')
        #        client.loop_stop()

        #Done

    finally:
        # all cleanup

        logging.info("Please wait for all processes to gracefully terminate and produce log files. Ctrl+C not recommended")
        
        if rtcp_flag == 0:
            broker.stop()

        #Executing the Stop commands
        for wkld in no_measured_wkld_vm:
            print(wkld)
            with open(f"./logs/{wkld[0]['wl_list'][0]['wl']}_{wkld[0]['wl_list'][0]['profile_name']}_{datetime.now().strftime('%Y_%m_%d_%H_%M_%S')}_stop_cmd.log",'w') as f:
                if wkld[0]['wl_list'][0]['stop_cmd']:
                    logging.info('Executing: '+wkld[0]['wl_list'][0]['stop_cmd'])
                    subprocess.run(wkld[0]['wl_list'][0]['stop_cmd'],stdout=f,shell=True,encoding='utf-8',universal_newlines=True)
        
        for wkld in measured_wkld_vm:   #for i,wkld in enumerate(workloads):
            print(wkld)
            with open(f"./logs/{wkld[0]['wl_list'][0]['wl']}_{wkld[0]['wl_list'][0]['profile_name']}_{datetime.now().strftime('%Y_%m_%d_%H_%M_%S')}stop_cmd.log",'w') as f:
                #subprocess.Popen(wkld[0]['wl_list'][0]['stop_cmd'],stdout=f,shell=True,encoding='utf-8',universal_newlines=True)
                if wkld[0]['wl_list'][0]['stop_cmd']:
                    logging.info('Executing: '+wkld[0]['wl_list'][0]['stop_cmd'])
                    subprocess.run(wkld[0]['wl_list'][0]['stop_cmd'],stdout=f,shell=True,encoding='utf-8',universal_newlines=True)
                time.sleep(1)
                
            
            with open(f"./logs/{wkld[1]['wl_list'][0]['wl']}_{wkld[0]['wl_list'][0]['profile_name']}_{datetime.now().strftime('%Y_%m_%d_%H_%M_%S')}stop_cmd.log",'w') as f:
                if wkld[1]['wl_list'][0]['stop_cmd']:
                    logging.info('Executing: '+wkld[1]['wl_list'][0]['stop_cmd'])
                    subprocess.run(wkld[1]['wl_list'][0]['stop_cmd'],stdout=f,shell=True,encoding='utf-8',universal_newlines=True)
                #subprocess.Popen(wkld[1]['wl_list'][0]['stop_cmd'],stdout=f,shell=True,encoding='utf-8',universal_newlines=True)
                time.sleep(1)
            
        if broker:
            broker.stop()

        if system_metrics:
            system_metrics.kill()

        if runner:
            runner.kill()

            # wait for processes to release file handles, else below file open for max fps fails
            time.sleep(1)

            for vm_id,wkld in only_measured.items():
                for k,grouped_wl in wkld.items():
                    if grouped_wl["calculate"] == "max_fps":
                        runner.calculate_fps(grouped_wl["wl_list"][0])
                        runner.density = "N.A"
                        break

                    logging.info(f"*********************************************************")
                    if not runner.execution_status:
                        logging.warning(f"wlc_bench execution status: FAIL")
                        logging.warning(f"Execution terminated due to a"
                                    f" failure (LWT/Terminated message from one of the WLs). "
                                    f"wlkd_density # is not accurate")
                    else:
                        logging.info(f"wlc_bench execution status: SUCCESS")

                    logging.info(f"WLC Density for measured WL: {grouped_wl['type']} = "
                             f"{grouped_wl['wkld_density']}")
                    logging.info(f"*********************************************************")

        logging.info("Done")


def setup_workloads(proxy, measured):
    """
    Parse and create proxy, measured wl lists, ready for spawning
    :param proxy: list of proxy wls from yaml
    :param measured: list of measured wls from yaml
    :return: list of proxy, measured wls
    """

    workloads = []

    # create objects for all wl types
    for wl in proxy["wl_list"]:
        wl["type"] = "proxy"
    
    #print(type(proxy))
    #print(measured, type(measured))
    workloads.append(proxy)
    
    if measured: 
        for conglomerate_wl, wl_list in measured.items():
        # if max_fps is set, ensure instances number == 1 at
        # conglomerate_wl level AND wl_list level
        # e.g: rpos_retail_high:
        #         instances: 1
        #         wl_list:
        #           [
        #             {...
        #               instances: 1,...
        #       }
            wl_list["type"] = f"measured_{conglomerate_wl}"
            wl_list["wkld_density"] = 0
            wl_list["isExist"] = True

            # Flag indicating if measured WL runs were successful or not
            wl_list["is_success"] = True

            if "calculate" in wl_list and \
                    wl_list["calculate"] == "max_fps" and \
                    (len(wl_list["wl_list"]) > 1 or
                    wl_list["wl_list"][0]["instances"]) > 1:
                logging.error("calculate:max_fps has been set. # of WLs in "
                          "conglomerate WLs can only be 1. Please check yml file")
                exit(1)

            for each_wl in wl_list["wl_list"]:
                each_wl["type"] = f"measured_{conglomerate_wl}"
            workloads.append(wl_list)
    
    else:
        measure_check = {}
        measure_check["type"] = "measured"
        measure_check["isExist"] = False
        workloads.append(measure_check)

    return workloads


def get_mqtt_yml_values(mode, parsed_file, parser):
    """
    Get MQTT config values from yml file
    :param mode: mqtt key from yml file
    :param parsed_file: parsed object from parse() API
    :param parser: YML parser object handle
    :return: tuple of values
    """
    # setup broker
    mqtt = parser.get(mode, parsed_file)
    mqtt_host = parser.get("host", mqtt) or "locahost"
    mqtt_port = parser.get("port", mqtt) or 1883

    return mqtt_host, mqtt_port


def get_yml_values(mode, parsed_file, parser):
    """
    Extract all values from parsed YML object
    :param mode: WLC mode, ex: breakpoint_serial
    :param parsed_file: parsed object from parse() API
    :param parser: YML parser object handle
    :return: tuple of values
    """
    # mode dictionary
    mode = parser.get(mode, parsed_file)

    # get log level
    log_level = parser.get("loglevel", mode) or 0

    # get settling time
    settling_time = parser.get("settling_time", mode)

    return settling_time, log_level


def setup_logging(log_level):
    """
    Setup logger formats, setup logging to stdout and file
    :param log_level: log level, 0-info, 1-debug
    :return: None
    """
    # remove old logs dir before starting
    shutil.rmtree("./logs", ignore_errors=True)
    Path("./logs/").mkdir(parents=True, exist_ok=True)

    # all logs in ./logs dir
    file_name = f"./logs/log_stdout_wlc_bench_{datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}.log"
    out_format = "\r%(asctime)-15s :: %(module)-15s :: line:%(lineno)-10d :: %(levelname)-10s :: %(message)-s\r"
    formatter = coloredlogs.ColoredFormatter(out_format)

    file_handle = logging.FileHandler(file_name)
    sream_handle = logging.StreamHandler()

    file_handle.setFormatter(formatter)
    sream_handle.setFormatter(formatter)

    if log_level == 1:
        logging.basicConfig(
            level=logging.DEBUG,
            force=True,
            format=out_format,
            handlers=[
                file_handle,
                sream_handle
            ]
        )
    else:
        logging.basicConfig(
            level=logging.INFO,
            force=True,
            format=out_format,
            handlers=[
                file_handle,
                sream_handle
            ]
        )
    logging.info("Removed old logs directory")

rtcp_flag = 0
if __name__ == "__main__":
    main()
