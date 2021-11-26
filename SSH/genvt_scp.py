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


import paramiko
import sys
import yaml
from scp import SCPClient
f =open(f"VM_Files/vm{sys.argv[1]}_ipaddress.yml",mode='r',encoding = 'utf-8') 
read_data = yaml.full_load(f)
ssh = paramiko.SSHClient()
ssh.load_host_keys('/home/wlc/.ssh/known_hosts')
ssh.load_system_host_keys()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(read_data['guest_ip'],username = read_data['login'], password = read_data['password'],allow_agent=False)
guest_ip = read_data['guest_ip']
login = read_data['login']
host_password = read_data['password']

# SCPCLient takes a paramiko transport as an argument
scp = SCPClient(ssh.get_transport())

# Uploading the 'test' directory with its content in the
# '/home/user/dump' remote directory
scp.put(sys.argv[2], recursive=True, remote_path=sys.argv[3])

scp.close()
