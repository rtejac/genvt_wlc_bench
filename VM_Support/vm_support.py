import os
import yaml
import libvirt
import time
import getpass
import netifaces as ni
import xml_parsing_main as xml
from yml_parser import YAMLParser
from VM_Support import libvirt_console as console_vm


class VM():

    def __init__(self,vm_name,os_name,os_image,vm_index,proxy,measured):

        self.vm_name = vm_name
        self.os_name = os_name
        self.os_image = os_image
        self.vm_index = vm_index
        self.proxy = proxy
        self.measured = measured

    
    def proxy_init_exec(self):

        wl_setup = self.proxy['wl_list'][0]['wl_setup']
        if wl_setup:
            print("\rProxy wkld transfer command is being executed, and it might take some time based on the steps included in it",'\r')
            os.system(wl_setup)
            print(f'\rProxy wkld transfered to vm_{self.vm_index} ')
        else:
            print(f'\rNo Transfer command for vm_{self.vm_index} Proxy, Skipping this\r')
    
    
    def measured_init_exec(self):
        
        wl_setup = self.measured['indu_hmi_high']['wl_list'][0]['wl_setup']
        if wl_setup:
            print("\rMeasured wkld transfer command is being executed, and it might take some time based on the steps included in it",'\r')
            os.system(wl_setup)
            print(f'\rMeasured wkld transfered to vm_{self.vm_index}')
        else:
            print(f'\rNo Transfer command for vm_{self.vm_index} Measured, Skipping this\r')
    
    
    
    
    
    def create_vm(self):

        print(f"\rCreating a {self.os_name} VM with image {self.os_image}, VM name {self.vm_name}\r")
        try:
            conn = libvirt.open('qemu:///system')
        except libvirt.libvirtError as e:
            print(repr(e),file=sys.stderr)
            exit(1)

        username = getpass.getuser()
    
        #geting ip address of host machine
        ni.ifaddresses('virbr0')
        ip = ni.ifaddresses('virbr0')[ni.AF_INET][0]['addr']
        #print("\n\rIP Address of Host machine:{}".format(ip))

        try:
            xml.vm_xml_create(self.vm_index,self.vm_name,self.os_image)
            f_xml = open(f'vm_{self.vm_index}.xml','r')
            dom = conn.createXML(f_xml.read(),0)
        except Exception as e:
            print("Can Not boot guest domain {file=sys.stderr}")
            print(e)

        uuid = dom.UUIDString()
        print(f"\rsystem : {dom.name()}  booted, file=sys.stderr and might take sometime for the GUI to show up (20 Seconds)\r")
    
        #f = open("guest.yml", mode = 'w', encoding = 'utf-8')
        #vm_0 = {'vm_name':self.vm_name,'vm_uuid':uuid,'host_ip':ip}
        # Aregument file creation for guest
        #for x,y in vm_0.items():
        #    f.write(f"{x} : {y}\n")
        #f.close()
    
        time.sleep(20)
    
        #console call for created VM
        console = console_vm.Console('qemu:///system',dom.name(), vm_index = self.vm_index)
        console.stdin_watch = libvirt.virEventAddHandle(0, libvirt.VIR_EVENT_HANDLE_READABLE, console_vm.stdin_callback, console)
        while console_vm.check_console(console):
            libvirt.virEventRunDefaultImpl()
        #dom.destroy()
        conn.close()
