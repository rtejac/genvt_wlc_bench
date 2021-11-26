#!/usr/bin/env python3
# consolecallback - provide a persistent console that survives guest reboots
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
import time
import re
import sys
import logging
import libvirt
import tty
import termios
import atexit
from VM_Support import libvirt_ipaddress as guest_ip
from argparse import ArgumentParser
from typing import Optional  # noqa F401
import subprocess

login_data = []
un_data = []
def reset_term() -> None:
    termios.tcsetattr(0, termios.TCSADRAIN, attrs)


def error_handler(unused, error) -> None:
    # The console stream errors on VM shutdown; we don't care
    if error[0] == libvirt.VIR_ERR_RPC and error[1] == libvirt.VIR_FROM_STREAMS:
        return
    logging.warn(error)


class Console(object):
    def __init__(self, uri: str, name: str,vm_index: int) -> None:
        self.uri = uri
        self.name = name
        self.connection = libvirt.open(uri)
        self.domain = self.connection.lookupByName(name)
        self.state = self.domain.state(0)
        self.connection.domainEventRegister(lifecycle_callback, self)
        self.stream = None  # type: Optional[libvirt.virStream]
        self.run_console = True
        self.stdin_watch = -1
        #self.args = args
        self.vm_index = vm_index
        self.login_flag = 1
        self.cmd_flag = 1
        self.f = open(f"VM_Files/vm{self.vm_index}_ipaddress.yml", mode = 'w',encoding = 'utf-8')
        logging.info("%s initial state %d, reason %d",
                     self.name, self.state[0], self.state[1])



def check_console(console: Console) -> bool:
    if (console.state[0] == libvirt.VIR_DOMAIN_RUNNING or console.state[0] == libvirt.VIR_DOMAIN_PAUSED):
        if console.stream is None:
            console.stream = console.connection.newStream(libvirt.VIR_STREAM_NONBLOCK)
            console.domain.openConsole(None, console.stream, 0)
            console.stream.eventAddCallback(libvirt.VIR_STREAM_EVENT_READABLE, stream_callback, console)
    else:
        if console.stream:
            console.stream.eventRemoveCallback()
            console.stream = None

    return console.run_console



# most hardcoded part of code need to make it generalize 
def stdin_callback(watch: int, fd: int, events: int, console: Console) -> None:
    readbuf = os.read(fd, 1024)
    if readbuf.startswith(b"+"):
        console.run_console = False
        return

    if console.stream:
        console.stream.send(readbuf)


def stream_callback(stream: libvirt.virStream, events: int, console: Console) -> None:
    try:
        assert console.stream
        received_data = console.stream.recv(1024)
    except Exception:
        return
    os.write(0, received_data)
    try:
        login_data.append(received_data.decode('utf-8'))
    except UnicodeDecodeError:
        print(" ")

    char_data = ''.join(login_data)
    vm_name = console.name
    #if stream_callback.login_flag == 1:
    if console.login_flag == 1:
        for login_str in char_data.rsplit():
            if login_str == 'login:' and stream_callback.login_flag == 1:
                console.stream.send(b"wlc\n")
                time.sleep(1)
                console.stream.send(b"wlc123\n")
                login_data.clear()
                #stream_callback.login_flag = 2
                console.login_flag = 2
    else:
        pass
        #print('\rlogin flag: ',stream_callback.login_flag,'\r')
    
    if console.cmd_flag == 1:
        for login_str in char_data.rsplit():
            ansi_escape_8bit = re.compile(br'(?:\x1B[@-Z\\-_]|[\x80-\x9A\x9C-\x9F]|(?:\x1B\[|\x9B)[0-?]*[ -/]*[@-~])')
            result = ansi_escape_8bit.sub(b'', login_str.encode())
            if result.decode() == 'wlc@ubuntu-vm:~$' and stream_callback.cmd_flag == 1:
                #console.stream.send(b"echo wlc123 | sudo -S mount -t 9p -o trans=virtio,version=9p2000.L mytag /mnt\n")
                #console.stream.send(b"echo DISPLAY env value: $DISPLAY \n")
                ipaddress = guest_ip.Guest_IPAddress(console.domain)
                vm_info = {'vm_name':vm_name,'guest_ip':ipaddress,'login':'wlc','password':'wlc123'}
                
                # Aregument file creation for guest
                for x,y in vm_info.items():
                    console.f.write(f"{x} : {y}\n")
                console.f.close()
                time.sleep(1)
                logging.info('IP Address of the created VM is written to yaml yaml file')
                login_data.clear()
                console.cmd_flag = 2
                console.run_console = False
                return
    else:
        pass
        #print('\rcommand flag: ',stream_callback.cmd_flag,'\r')



def lifecycle_callback(connection: libvirt.virConnect, domain: libvirt.virDomain, event: int, detail: int, console: Console) -> None:
    console.state = console.domain.state(0)
    logging.info("%s transitioned to state %d, reason %d",
                 console.uuid, console.state[0], console.state[1])

# main
libvirt.virEventRegisterDefaultImpl()
libvirt.registerErrorHandler(error_handler, None)

atexit.register(reset_term)
attrs = termios.tcgetattr(0)
tty.setraw(0)
stream_callback.login_flag = 1
stream_callback.cmd_flag = 1

