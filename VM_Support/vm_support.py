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
import yaml
import libvirt
import time
import getpass
import logging
import netifaces as ni
import xml_parsing_main as xml
from yml_parser import YAMLParser
from VM_Support import libvirt_console as console_vm


class VM():

    def __init__(self,vm_name,os_name,os_image,vm_index,proxy,measured,gpu_pass,ram,cpu):

        self.vm_name = vm_name
        self.os_name = os_name
        self.os_image = os_image
        self.vm_index = vm_index
        self.proxy = proxy
        self.measured = measured
        self.gpu_pass = gpu_pass
        self.ram = ram*1024*1024
        self.cpu = cpu

    
    def proxy_init_exec(self):
        
        wl_setup = self.proxy['wl_list'][0]['wl_setup']
        if wl_setup:
            logging.info("Proxy wkld transfer command is being executed, and it might take some time based on the steps included in it")
            os.system(wl_setup)
            logging.info(f'Proxy wkld transfered to vm_{self.vm_index} ')
        else:
            logging.info(f'No Transfer command for vm_{self.vm_index} Proxy, Skipping this')
    
    
    def measured_init_exec(self):
        
        if not self.measured:
            return

        wl_setup = self.measured['indu_hmi_high']['wl_list'][0]['wl_setup']
        if wl_setup:
            logging.info("Measured wkld transfer command is being executed, and it might take some time based on the steps included in it")
            os.system(wl_setup)
            logging.info(f'Measured wkld transfered to vm_{self.vm_index}')
        else:
            logging.info(f'No Transfer command for vm_{self.vm_index} Measured, Skipping this')
    
    
    
    def create_vm(self):

        try:
            conn = libvirt.open('qemu:///system')
        except libvirt.libvirtError as e:
            print(repr(e),file=sys.stderr)
            exit(1)

        try:
            dom = conn.lookupByName(self.vm_name)

            if dom.isActive():
                logging.info(f'VM {self.vm_name} is already running, Not creating again')
                return
        except:
            logging.info(f'No previous instance of {self.vm_name} VM is found...')
        
        logging.info(f"Creating a {self.os_name} VM with image {self.os_image}, VM name {self.vm_name} with {self.cpu} CPUs and {self.ram/(1024*1024)} GB of RAM")
        username = getpass.getuser()
    
        #geting ip address of host machine
        ni.ifaddresses('virbr0')
        ip = ni.ifaddresses('virbr0')[ni.AF_INET][0]['addr']

        try:
            #Include this line in future, diabling as the xmls are already created...
            xml.vm_xml_create(self.vm_index,self.vm_name,self.os_image,self.gpu_pass,self.ram,self.cpu)
            f_xml = open(f'VM_Files/vm_{self.vm_index}.xml','r')
            dom = conn.createXML(f_xml.read(),0)
        except Exception as e:
            logging.error("Can Not boot guest domain {file=sys.stderr}")
            logging.error(e)

        uuid = dom.UUIDString()
        logging.info(f"{dom.name()}  booted, might take sometime to show up")
    
        time.sleep(20)
    
        #console call for created VM
        console = console_vm.Console('qemu:///system',dom.name(), vm_index = self.vm_index)
        console.stdin_watch = libvirt.virEventAddHandle(0, libvirt.VIR_EVENT_HANDLE_READABLE, console_vm.stdin_callback, console)
        while console_vm.check_console(console):
            libvirt.virEventRunDefaultImpl()
        conn.close()



def validate_yaml(parser,mode):
    
    
    gpu_pass = 0
    measured = 0
    vm_names = []
    vm_paths = []

    for k,v in mode.items():
        if 'vm' in k:
            current_vm_info = parser.get(k, mode)
            vm_names.append(current_vm_info['vm_name'])
            vm_paths.append(current_vm_info['os_image'])

            if current_vm_info['gpu_passthrough'] != 0:
                gpu_pass += 1
            try:    
                if current_vm_info['measured_wl']:
                    measured += 1
            except:
                pass
    
    for i in vm_names:
        if vm_names.count(i) > 1:
            logging.error('More than 1 VM has same name ... INVALID yaml file')
            exit()

    for i in vm_paths:
        if vm_paths.count(i) > 1:
            logging.error('More than 1 VM has same path ... INVALID yaml file')
            exit()
    
    if gpu_pass != 1:
        logging.error('More than 1 VM has GPU pass through... INVALID yaml file')
        exit()
    if measured > 1:
        logging.error('None or more than 1 VM has measured workload mentioned... INVALID yaml file')
        exit()
