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
import os
import shutil
import sys
import time
from getpass import getpass
from pathlib import Path

import coloredlogs

from broker import Broker
from system_metrics import SystemMetrics
from wl_launcher import WlLauncher
from yml_parser import YAMLParser

#from vm_create import create_vm
from vm_class import VM



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

    # get all proxy, measured wl details

    #Teja edit

    #Gettitng the VM related wl details in a list
    p = {}
    m = {}
    for k,v in mode.items():
        if 'vm' in k:
            cur_vm = parser.get(k, mode)
            p[k] = cur_vm['proxy_wl']
            p[k]['type'] = 'proxy'
            m[k] = cur_vm['measured_wl']
            vm_index = int(k.split('_')[1]) #to get 0,1 from vm_0,vm_1 and use them for creating VM index
            
            vm_0 = VM(cur_vm['vm_name'],cur_vm['os_name'],cur_vm['os_image'],vm_index,p[k],m[k])
            vm_0.create_vm()
            vm_0.proxy_init_exec()

    workloads = []
    for k,v in p.items():
        workloads.append(setup_workloads(p[k], m[k]))
    
    #exit()
    #Done
    

    #Original code
    
    #proxy = parser.get("proxy_wl", mode)
    #proxy["type"] = "proxy"
    #measured = parser.get("measured_wl", mode)

    #workloads = setup_workloads(proxy, measured)
    
    #Done
    
    # get mqtt values
    mqtt_host, mqtt_port = get_mqtt_yml_values("mqtt", parsed_file, parser)
    
    #Teja edit
    with open('../ipaddress.yml',mode='r',encoding='utf-8') as f:
        data = yaml.full_load(f)
    # setup broker
    broker = Broker(data['guest_ip'], mqtt_port)

    #Done

    #broker = Broker(mqtt_host, mqtt_port)
    broker.start()

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
        for i,wkld in enumerate(workloads):
            #print(f'\rLaunchig WLs in VM {i}, WL details',wkld)
            # start workload launcher
            runner = WlLauncher(wkld, settling_time, system_metrics, broker)
            runner.run()
        #Done

    finally:
        # all cleanup
        if broker:
            broker.stop()

        logging.info("Please wait for all processes to gracefully terminate and produce log files."
                     " ctrl+c not recommended")
        if system_metrics:
            system_metrics.kill()

        if runner:
            runner.kill()

            # wait for processes to release file handles, else below file open for max fps fails
            time.sleep(1)

            for vm_id,wkld in m.items():
                for k, grouped_wl in wkld.items():
                    #print(grouped_wl)
                    #grouped_wl = grouped_wl[0]
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
    #print(type(measured))
    workloads.append(proxy)
    
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
    
    #print('workloads in function: ',workloads)
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
    file_name = f"./logs/log_stdout_wlc_bench_{datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}.log"
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


if __name__ == "__main__":
    main()
