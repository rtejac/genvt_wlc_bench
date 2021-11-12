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

import os
import glob
import json
import re
import statistics
import subprocess
import time

import psutil

from broker import *

disconnect = False
wl_count = 0


class WlLauncherException(Exception):
    """
    Class for WLC runner exceptions
    """
    pass


class WlLauncher(object):
    """
    Launch WLC processes

    """

    def __init__(self,
                 wl_dict,
                 settling_time,
                 metrics_handle, broker):
        """
        Init WLC runner
        :param wl_dict: workload dict created when wlc_bench gets launched holding all attributes of that wl
        :param settling_time: time in secs to wait until app loads
        :param broker: handle to MQTT broker client
        """
        self.wl_list = wl_dict

        self.settling_time = settling_time
        self.wl_proc_obj = {}

        self.broker = broker

        self.metrics_handle = metrics_handle

        self.density = 0

        self.wl_status = {}

        self.execution_status = True

    def initialize(self):
        """
        Initialize runner with topics to subscribe to
        :return: None
        """

        def _on_status_change(topic, payload):
            """
            Status change callback
            :param topic: input topic string
            :param payload: received payload
            :return: disconnect from app if connected msg not received, else continue
            """
            global disconnect, wl_count
            # topic is <app>/<pid>/kpi/status, synbench/122/kpi/status
            # message is {"timestamp":"2021-10-05 16:45:11", "message":"connected"}
            # message is {"timestamp":2021-10-05 16:45:11, "message":"terminated"}
            # message is {"timestamp":"null", "message":"lwt"}
            val = topic.split("/")

            try:
                #print('Payload message: ',payload)
                if payload == 'lwt':
                    #disconnect = True
                    #self.execution_status = False
                    return

                payload = json.loads(payload)
                logging.info(f"[Received] on : {topic} message: {payload['message']}")
                if payload["message"] != "connected":
                    logging.error(f"Process {val[0]} with parent PID:{val[1]} disconnected from MQTT!")
                    disconnect = True
                    self.execution_status = False
                else:
                    self.wl_status[wl_count] = True

            except json.decoder.JSONDecodeError as ex:
                logging.error(f"Received message not in JSON format. Received payload from"
                              f" process {val[0]} with parent PID:{val[1]} is : {payload}. "
                              f"Exception message is {str(ex)}")
                pass
                #disconnect = True
                #self.execution_status = False

        def _on_kpi_error(topic, payload):
            """
            KPI error callback
            :param topic: input topic string
            :param payload: received payload
            :return: disconnect from app if KPI err msg received, else continue
            """
            global disconnect

            try:
                payload = json.loads(payload)
                logging.debug(f"[Received] on : {topic} message: {payload}")

                # topic is <app_name>/<pid>/kpi/<error,warning etc.>/<error #>, synbench/122/kpi/error/1
                val = topic.split("/")

                logging.error(f"KPI ERROR #{val[4]} detected "
                              f"on process:{val[0]},{val[1]} "
                              f"message: {payload['message']}")
                disconnect = True

            except json.decoder.JSONDecodeError as ex:
                logging.error(f"Could not parse raw data: {payload}")

        try:
            logging.info(f"Subscribe to initial topics with callbacks")
            self.broker.subscribe("+/+/kpi/error/+", _on_kpi_error)
            self.broker.subscribe("+/+/kpi/status", _on_status_change)

        except Exception as e:
            raise BrokerException(e)

    def run(self):
        """
        Start all subprocesses, call initialize() for init broker with topics
        :return: None
        """
        global wl_count
        self.initialize()

        # workload object wl_dict(wl, profile_name, start_cmd, stop_cmd, instances, calculate)
        for grouped_wl in self.wl_list:

            # no nested sub listed in proxy, so start them directly
            #print('grouped_wl in wl_launcher',grouped_wl,type(grouped_wl))
            if "proxy" in grouped_wl["type"]:
                for each_wl in grouped_wl["wl_list"]:
                    self.create_subprocess(each_wl)

            else:
                # there are nested sub lists in measured wl
                # since we have grouped/conglomerate wls defined
                # parse through each list and sub list
                for x in range(grouped_wl["instances"]):
                    logging.info(f"Running measured round ({x + 1}"
                                 f" of {grouped_wl['instances']}) for {grouped_wl['type']}")
                    for each_wl in grouped_wl["wl_list"]:
                        self.create_subprocess(each_wl)

                grouped_wl["wkld_density"] = self.density
                self.density = 0

        if self.wl_proc_obj is not None:
            for k, v in self.wl_proc_obj.items():
                logging.info(f"Created: {len(v)} of {k}")

    def create_subprocess(self, wl):
        """
        Create subprocesses using Popen API
        :param wl: workload dict from yml file
        :return: None, breaks if KPI error exception encountered
        """
        global wl_count

        wl_name = wl["wl"]
        wl_type = wl["type"]
        profile_name = wl["profile_name"]

        init_cmd = None
        if "init_cmd" in wl and wl["init_cmd"] is not None:
            init_cmd = wl["init_cmd"]

        cmd = wl["start_cmd"]

        instances_num = wl["instances"]
        kpi_window = wl["kpi_window"]

        # create dict holding all data about wls instantiated till now
        # its a dict with lists as values
        # dict(proxy-high: [list of pids], measured-high: [list of pids]

        workload_name = f"{wl_name}-{wl_type}-{profile_name}"
        if f"{workload_name}" not in self.wl_proc_obj:
            self.wl_proc_obj[f"{workload_name}"] = []

        for x in range(instances_num):
            # global count of all workloads started
            wl_count = wl_count + 1

            try:
                # run any init commands if init_cmd key has been set in yml file
                if init_cmd:
                    #print(f'\r{init_cmd}\r')
                    with open(f"./logs/log_stdout_{workload_name}_wl_{wl_count}_init_cmd.log", "w") as init_file:
                        subprocess.Popen(init_cmd,
                                         stdout=init_file,
                                         shell=True, encoding="utf-8", universal_newlines=True)
                        #pass
                    logging.info("Ran workload initialization commands")

                logging.info("Pause subscription on KPI errors channel to handle new WL error spikes")
                self.broker.unsubscribe("+/+/kpi/error/+")

                if cmd:
                    #print(f'\r{cmd}\r')
                    with open(f"./logs/log_stdout_{workload_name}_wl_{wl_count}.log", "w") as out_file:
                        proc = subprocess.Popen(cmd,
                                                stdout=out_file,
                                                shell=True, encoding="utf-8", universal_newlines=True)
                else:
                    logging.warning("No start_cmd specified. Skipping this WL"
                                    f"{workload_name}_wl_{wl_count}")
                    continue

                logging.info(f"{workload_name} instance: wl_#{wl_count} CMD:{cmd} PID:{proc.pid}")

                # append all proc objs to dict
                self.wl_proc_obj[f"{workload_name}"].append(proc)

                logging.info(f"Wait for settling time of {self.settling_time}s")
                self.wait(self.settling_time)

                if wl_count not in self.wl_status:
                    logging.warning(f"Not received 'connected' message from "
                                    f"{workload_name}."
                                    "\tPlease check if workload is connected to the MQTT broker."
                                    "\tCheck log file for any errors."
                                    "\tOr your WL may not be publishing messages on MQTT."
                                    "\tWL Density calculations will thus be incorrect")

                # capture metrics if enabled
                if self.metrics_handle.is_capture_metrics:
                    self.metrics_handle.collect_store(self.wl_proc_obj,
                                                      wl_type)

                logging.info("Restart subscription on KPI errors channel")
                self.broker.subscribe("+/+/kpi/error/+",
                                      self.broker.subscription_list["+/+/kpi/error/+"])

                logging.info(f"Watch for KPI errors in KPI window duration of: {kpi_window}s")
                self.wait(kpi_window)

                # increment wlc density value if measured wl only - proxy wl not counted for density
                if "measured" in wl_type:
                    self.density = self.density + 1

            except WlLauncherException as e:
                logging.error(e)
                break

            finally:
                time.sleep(1)

    @staticmethod
    def calculate_fps(workload):
        """
        Calculate avg FPS from wl log file
        :param workload: wl dict from yml file
        :return: None, calculates and print fps as float
        """
        # TODO: move to mqtt based impl later
        # max fps for 1 measured wl only

        # for max_fps, there will always be one measured file to open
        path = f"./logs/log_stdout_{workload['wl']}_{workload['type']}_" \
               f"{workload['profile_name']}_wl_*"

        for filename in glob.glob(path):
            with open(filename) as f:
                content = f.readlines()
                avg = []

                for line in content:
                    if "fps." in line:
                        matches = re.findall(r"[+-]?\d+\.\d+", line)
                        if matches:
                            avg.append(float(matches[-1]))

                if avg:
                    mean_fps = statistics.mean(avg)

                    logging.info(f"*********************************************************")
                    logging.info(f"Max FPS for wl_{workload['type']}"
                                 f"_{workload['wl']}-{workload['profile_name']} "
                                 f"= {mean_fps:.3f}")
                    logging.info(f"*********************************************************")

    @staticmethod
    def wait(time_period):
        """
        Wait for specified time with countdown
        :param time_period: time in secs
        :return: None
        """
        global disconnect
        for i in range(1, time_period + 1, 1):
            if disconnect:
                raise WlLauncherException("KPI error encountered. Closing wlc_bench")

            print(f"Wait for {i}", end="\r")
            time.sleep(1)

    def kill(self):
        """
        Kill runner, kill all processes spawned by runner
        :return: None
        """
        # terminate all spawned app processes
        for k, v in self.wl_proc_obj.items():
            for proc in v:
                try:
                    out, err = proc.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    if psutil.pid_exists(proc.pid):
                        p = psutil.Process(proc.pid)

                        # terminate child processes as well, if running many cmds through script
                        children = p.children(recursive=True)
                        p.kill()

                        for child in children:
                            child.kill()

                    out, err = proc.communicate()

        # terminate any docker containers if spawned, needs stop_cmd param set in yml file
        for wl in self.wl_list:
            for each_wl in wl["wl_list"]:
                if each_wl["stop_cmd"]:
                    print("\r"+each_wl["stop_cmd"]+"\r")
                    subprocess.Popen(each_wl["stop_cmd"], stdout=subprocess.PIPE, shell=True)

        logging.info("Terminated all spawned processes")
