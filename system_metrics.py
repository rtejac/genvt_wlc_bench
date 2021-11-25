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
import logging
import os
import shutil
import subprocess
import time

import pandas as pd
from openpyxl import load_workbook

import tool_utils


class SystemMetricsException(Exception):
    """
    Class for system metrics exceptions
    """
    pass


class SystemMetrics(object):
    """
    System Metrics class for defining, creating commands for metrics tools like
    top, gpu_top etc. Also collects and stores data in suitable log files
    """

    def __init__(self, is_capture=False):
        """
        Init system metrics object
        :param is_capture: flag True/False to indicate if system metrics to be captured or not
        """
        self._sudo_password = None
        self.metrics_log = "./logs/system_metrics.log"
        self.is_capture_metrics = is_capture
        self.tool_bank = []

    @property
    def sudo_password(self):
        """
        Property to get/set password to run sudo cmds
        :return: sudo password
        """
        return self._sudo_password

    @sudo_password.setter
    def sudo_password(self, x):
        """
        Setter
        :param x: value to be set
        :return: None
        """
        self._sudo_password = x

    @staticmethod
    def check_tool_exists(tool, loc=None):
        """
        Check if tool exists/has been installed
        :param tool: tool name in string
        :param loc: location/path to search for tool
        :return: True if yes, else False
        """
        if shutil.which(tool, path=loc) is None:
            logging.warning(f"Please install {tool}. Skipping {tool} metrics")
            return False

        return True

    def collect_store(self, pids=None, wl_type=None):
        """
        Function to collect and write all system metric measurements in log files
        :param pids: dict of proxy, measured wls -
                     dict(proxy-high: [list of pids], measured-high: [list of pids]
        :param wl_type: type of workload, proxy or measured
        :return: None
        """

        total = total_proxy = total_measured = 0
        proxy_str = measured_str = ""

        logging.debug(pids)

        # find total # of proxy wl and measured wl instantiated till now
        # find what all types of proxy, measured wl have been started
        if pids is not None:
            for k, v in pids.items():
                if "proxy" in k:
                    total_proxy += len(v)
                    proxy_str += f"{k}, "
                if "measured" in k:
                    total_measured += len(v)
                    measured_str += f"{k}, "

                total += len(v)

        if wl_type is None:
            ret_val = ["None", "None", "System Idle"]
            log = "system idle"

        elif wl_type == "proxy":
            ret_val = [f"{total_proxy} * ({proxy_str})", "None", total_proxy]
            log = f"{total_proxy} proxy instance ({proxy_str})"

        else:
            ret_val = [f"{total_proxy} * ({proxy_str})", f"{total_measured} * ({measured_str})", total]
            log = f"{total} instances = {total_proxy} * ({proxy_str}) + {total_measured} * ({measured_str})"

        df_cols = ["Proxy WL", "Measure WL", "Total # WL"]

        # write data in system_metrics log file
        metrics_file = open(self.metrics_log, "a+")
        metrics_file.write(f"{log}:\n")

        for tool in self.tool_bank:

            # add the CPU usage
            if "cpu" in tool["type"]:

                cpu_data = tool_utils.get_cpu_top_data()

                # if top data > 0, include in excel - if any errors from top, skip
                if cpu_data >= 0:
                    ret_val.append(cpu_data)
                    metrics_file.write(f"\tCPU usage {ret_val[3]}\n")

                    df_cols.append("CPU Usage")

            # add the GPU usage
            if "gpu" in tool["type"]:

                gpu_data = tool_utils.get_gpu_top_data()

                # if gpu data list got populated, include in excel - if any errors, skip
                if gpu_data:
                    metrics_file.write(f"\tGPU freq {gpu_data[0]} Mhz\n")
                    metrics_file.write(f"\tGPU render {gpu_data[1]}\n")

                    ret_val.append(f"{gpu_data[0]} Mhz")
                    ret_val.append(gpu_data[1])

                    df_cols.append("GPU Freq")
                    df_cols.append("GPU Render")

            # add the memory data
            if "memory" in tool["type"]:

                pcm_data = tool_utils.get_pcm_data()

                if pcm_data:
                    ret_val = ret_val + [f"{pcm_data[0]} GBytes", f"{pcm_data[1]} GBytes", f"{pcm_data[2]} J"]

                    metrics_file.write(f"\tMemory read {pcm_data[0]} GBytes\n")
                    metrics_file.write(f"\tMemory write {pcm_data[1]} GBytes\n")
                    metrics_file.write(f"\tCPU energy {pcm_data[2]} J\n")

                    df_cols = df_cols + ["Memory read", "Memory write", "CPU energy"]

            # add the power data
            if "power" in tool["type"]:

                # collect socwatch per workload
                self.start(tool)
                power_data = tool_utils.get_cpu_socwatch_power(total)

                if power_data:
                    ret_val = ret_val + [f"{power_data[0]} mW", f"{power_data[1]} mJ"]

                    metrics_file.write(f"\tSOC Avg Power {power_data[0]} mW")
                    metrics_file.write(f"\tSOC Total Power {power_data[1]} mJ")

                    df_cols = df_cols + ["SOC Avg Power", "SOC Total Power"]

            # add pdu data if defined:
            if "pdu" in tool["type"]:

                pdu_data = tool_utils.get_pdu_target_power(tool)

                if pdu_data:
                    ret_val.append(f"{pdu_data} A")

                    df_cols.append("PDU Power")

                    metrics_file.write(f"\tPDU Power {pdu_data} A")

        metrics_file.write("\n")
        metrics_file.close()

        self.save_to_xls(df_cols, ret_val)

    def save_to_xls(self, df_cols, ret_val):
        """
        Saves values to xls
        :param df_cols: pandas dataframe
        :param ret_val: list of values
        :return: None
        """
        # open mode for excel file (write / append)
        xls_file = f"{self.metrics_log}.xlsx"

        try:
            df = pd.DataFrame([ret_val], columns=df_cols)

            if os.path.exists(xls_file) and os.path.isfile(xls_file):

                with pd.ExcelWriter(xls_file, engine="openpyxl", mode="a") as writer:
                    writer.book = load_workbook(xls_file)
                    writer.sheets = dict((ws.title, ws) for ws in writer.book.worksheets)
                    reader = pd.read_excel(xls_file, engine="openpyxl")
                    df.to_excel(writer, sheet_name="System Metrics", index=False,
                                header=False, startrow=len(reader) + 1)

            else:

                with pd.ExcelWriter(xls_file) as writer:
                    df.to_excel(writer, sheet_name="System Metrics", index=False)
                    worksheet = writer.sheets["System Metrics"]
                    worksheet.set_column(0, 1, 40, writer.book.add_format(
                        {"align": "center", "valign": "vcenter", "text_wrap": True}))
                    worksheet.set_column(2, 11, 15, writer.book.add_format(
                        {"align": "center", "valign": "vcenter", "text_wrap": True}))

        except Exception as e:
            self.kill()
            logging.error(f"Could not populate excel file: {xls_file}")
            raise SystemMetricsException(e)

    def start(self, tool):
        """
        Start metrics tools as subprocesses in the background. Stores pids in dictionary
        :param tool tool obj created when wlc_bench gets launched holding all attributes of that tool
        :return: None
        """
        file_name = f"./logs/log_stdout_{tool['name']}.log"

        logging.info(f"Starting {tool['name']}, logs: {file_name}")

        with open(f"{file_name}", "w") as f:
            # if tool needs sudo, start with sudo, else regular
            if "is_sudo" in tool and tool["is_sudo"]:
                '''cmd.insert(0, "sudo")
                cmd.insert(1, "-S")

                proc = subprocess.Popen(cmd,
                                        stdin=subprocess.PIPE,
                                        stdout=file)
                proc.stdin.write(self._sudo_password.encode())'''

                # above popen() API causing problems for gpu top
                # when wlc_bench started for the first time on terminal, to be debugged later
                cmd = f"echo {self.sudo_password} | sudo -S {tool['start_cmd']} > {file_name} 2>&1 &"
                subprocess.call(cmd, shell=True)

            else:
                split_cmd = tool["start_cmd"].split(" ")
                proc = subprocess.Popen(split_cmd, stdout=f)

        # wait 2 seconds for metrics tools to start and log values correctly
        logging.debug("Sleeping for 2 seconds to ensure tool has started")
        time.sleep(2)

    def kill(self):
        """
        Terminates metrics tools
        :return: None
        """
        for tool in self.tool_bank:
            # kill all tools started by wlc bench
            proc = subprocess.Popen(["sudo", "-S", "pkill", "-x", f"{tool['name']}"],
                                    stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE)
            proc.stdin.write(self._sudo_password.encode())
            logging.info(f"Terminated metrics tool: {tool['name']}")
