import yaml
import libvirt
import xml_parsing_main as xml
import time
import getpass
import libvirt_console as console_vm
from yml_parser import YAMLParser
import netifaces as ni


def create_vm(os_image,vm_name,vm_count=0):

    try:
        conn = libvirt.open('qemu:///system')
    except libvirt.libvirtError as e:
        print(repr(e),file=sys.stderr)
        exit(1)

    username = getpass.getuser()
    
    #geting ip address of host machine
    ni.ifaddresses('virbr0')
    ip = ni.ifaddresses('virbr0')[ni.AF_INET][0]['addr']
    print("\n\rIP Address of Host machine:{}".format(ip))

    try:
        xml.vm_xml_create(vm_count,vm_name,os_image)
        f_xml = open(f'vm_{vm_count}.xml','r')
        dom = conn.createXML(f_xml.read(),0)
    except Exception as e:
        print("Can Not boot guest domain {file=sys.stderr}")
        print(e)

    uuid = dom.UUIDString()
    print(f"\rsystem : {dom.name()}  booted, file=sys.stderr")
    
    f = open("guest.yml", mode = 'w', encoding = 'utf-8')
    vm_0 = {'vm_name':vm_name,'vm_uuid':uuid,'host_ip':ip}
    # Aregument file creation for guest
    for x,y in vm_0.items():
        f.write(f"{x} : {y}\n")
    f.close()
    
    print(f"\rWaiting for the system to boot(20 seconds)\r")
    time.sleep(20)
    
    #console call for created VM
    console = console_vm.Console('qemu:///system',dom.name())
    console.stdin_watch = libvirt.virEventAddHandle(0, libvirt.VIR_EVENT_HANDLE_READABLE, console_vm.stdin_callback, console)
    while console_vm.check_console(console):
        libvirt.virEventRunDefaultImpl()
    #dom.destroy()
    conn.close()

