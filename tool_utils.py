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
import csv
import logging
import os
import shutil

import pexpect

'''
Set of utility functions for capturing tool data from various profiling tools
These functions were ported from the work done in src-kpi-runners repo

Authors:
Dinu Munteanu <marian.dinu-munteanu@intel.com>
Cosmin Albescu <cosmin.albescu@intel.com>

Contributors:
Bijalwan, AnkeshX <ankeshx.bijalwan@intel.com>

'''


def get_gpu_top_data(gpu_log_file="./logs/log_stdout_intel_gpu_top.log"):
    """
    Function to obtain GPU frequency and render usage from the intel_gpu_top log file
    :param gpu_log_file: log file where intel_gpu_top is stored,
    default value is ./logs/og_stdout_intel_gpu_top
    :return: array[int, float]: gpu usage, first element is gpu frequency, second element is gpu render
    """
    gpu_value = []

    # check if file exists
    if os.path.exists(gpu_log_file) and os.path.isfile(gpu_log_file):
        logging.debug("GPU intel_gpu_top file {gpu_log_file} exists, reading values")
    else:
        logging.error("GPU intel_gpu_top file {gpu_log_file} not found")
        return ["Nan", "Nan"]

    # Read last 3 lines of intel_gpu_top log in order to obtain GPU usage
    with open(gpu_log_file, "r") as f:
        line = f.read().splitlines()

        for gpu_line in line[-3:]:
            if gpu_line.split()[0].isdigit():
                gpu_freq = gpu_line.split()[1]
                gpu_render = gpu_line.split()[7]
                gpu_value = [gpu_freq, float(gpu_render) / 100]

    return gpu_value


def get_cpu_top_data(cpu_log_file="./logs/log_stdout_top.log"):
    """
    Function to obtain CPU usage from the top log file
    :param cpu_log_file: log file where top is stored, default value is ./logs/log_stdout_top
    :return: float, cpu overall usage (user + system) in absolute value
    """
    cpu_value = -1

    # check if file exists
    if os.path.exists(cpu_log_file) and os.path.isfile(cpu_log_file):
        logging.debug(f"CPU top file {cpu_log_file} exists, reading values")

    else:
        logging.error(f"CPU top file {cpu_log_file} not found")
        return "Nan"

    # read last 8 lines of the top log to obtain the CPU usage,
    # if top returns other process then self, then last lines read will be increased
    with open(cpu_log_file, "r") as cpu_log:
        cpu_lines = cpu_log.read().splitlines()[-8:]

        for line in cpu_lines:
            if line.startswith("%Cpu(s)"):
                # add sy, us, ni times from top
                cpu_value = float(line.split()[1]) + float(line.split()[3]) + float(line.split()[5])

    return float(cpu_value) / 100


def get_pcm_data(pcm_log_file='./logs/log_stdout_pcm.x.log'):
    """
    Function to obtain memory usage and cpu power consumption from the pcm.x log file
    :param pcm_log_file: log file where output is stored, default value is ./logs/log_stdout_pcm.x
    :return: array[float, float, float]: memory usage, first element is memory read,
             second value is memory write in Gbytes, third value is CPU power usage in Joules
    """
    # check if file exists
    if os.path.exists(pcm_log_file) and os.path.isfile(pcm_log_file):
        logging.debug(f"pcm.x log file {pcm_log_file} exists, reading values")

    else:
        logging.info("pcm.x log file {pcm_log_file} not found")
        return ["Nan", "Nan", "Nan"]

    # Read last 57 lines that are the available execution line in order to obtain memory usage
    pcm_value = []
    with open(pcm_log_file, "r") as pcm_log:
        pcm_lines = pcm_log.read().splitlines()[-57:]
        required_skt = False

        for line in pcm_lines:
            if line.startswith("MEM (GB)"):
                required_skt = True

            if required_skt and line.strip().startswith("SKT"):
                memory_read = line.strip().split()[2]
                memory_write = line.strip().split()[3]
                cpu_energy = line.strip().split()[5]
                pcm_value = [memory_read, memory_write, cpu_energy]
                required_skt = False

    return pcm_value


# TODO: not tested, no PDU connected, needs to be tested
def get_pdu_target_power(tool):
    """
    Function to obtain current power consumption of DUT from PDU
    :param tool tool obj created when wlc_bench gets launched holding all attributes of that tool
    :return: float: current power consumption value from PDU measured in Amps
    """

    # Display the current power consumption
    pdu = pexpect.spawn("ssh " + tool["username"] + "@" + tool["ip"])
    logging.debug("Connection to PDU established")

    pdu.expect("password: ")
    pdu.sendline(tool["password"])
    pdu.expect("> ")
    logging.debug("Password accepted")

    pdu.sendline(tool["start_cmd"])
    logging.debug("Sent power outlet get command")

    pdu.expect("> ")
    target_power = str(pdu.before).split("Reading:")[1].split()[0]
    logging.debug("Received output from PDU")

    return target_power


def get_cpu_socwatch_power(num_wl_started, socwatch_log_file="./SoCWatchOutput.csv"):
    """
    Function to obtain CPU average power and CPU total power from the Socwatch log file
    :param num_wl_started: total # of wl started till this point
    :param socwatch_log_file: log file where output is stored, default - ./logs/log_stdout_socwatch
    :return: array[int, int]: CPU power usage,
             first element is CPU average power (mW), second element is CPU total power (mJ)
    """
    # check if file exists
    if os.path.exists(socwatch_log_file) and os.path.isfile(socwatch_log_file):
        logging.debug(f"CPU socwatch file {socwatch_log_file} exists, reading values")

    else:
        logging.info(f"CPU socwatch file {socwatch_log_file} not found")
        return ["Nan", "Nan"]

    # Read the output csv from socwatch log in order to obtain CPU power
    metric_type = "Power"
    cpu_power = []
    with open(socwatch_log_file, "r") as csv_file:
        csv_data = csv.reader(csv_file, delimiter=",")

        for line in csv_data:
            row = [element.strip() for element in line]

            if metric_type in row:
                cpu_avg_power = row[2]
                cpu_total_power = row[3]
                cpu_power = [cpu_avg_power, cpu_total_power]

    # move all logs to ./logs directory always, keep source dir clean
    shutil.move(socwatch_log_file, f"./logs/{socwatch_log_file}_{num_wl_started}_wl_started.log")
    shutil.move("./SoCWatchOutput.sw2", "./logs/SocWatchOutput.sw2")

    return cpu_power
