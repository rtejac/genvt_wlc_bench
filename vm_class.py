import os
from subprocess import Popen
from vm_create import create_vm


class VM():

    def __init__(self,vm_name,os_name,os_image,vm_index,proxy,measured):

        self.vm_name = vm_name
        self.os_name = os_name
        self.os_image = os_image
        self.vm_index = vm_index
        self.proxy = proxy
        self.measured = measured

        #print("\rProvided VM details", self.vm_name,self.os_image,self.vm_index,self.proxy, self.measured)

    def proxy_init_exec(self):

        init_cmd = self.proxy['wl_list'][0]['init_cmd']
        #print('\r',init_cmd,'\r')
        os.system(init_cmd)
        print("initialisation is done")
    
    def create_vm(self):

        print(f"\rCreating a {self.os_name} VM with image {self.os_image}, VM name {self.vm_name}\r")
        #Logic to create VM
        create_vm(self.os_image,self.vm_name,self.vm_index)


    def measured_init_exec(self):
        pass
