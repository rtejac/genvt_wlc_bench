import os
import sys
import yaml
import paramiko
import getpass
from subprocess import Popen,run
import paho.mqtt.client as mqtt
from datetime import datetime


client = mqtt.Client()
client.connect("localhost",1883,60)
client.loop_start()


def send_msg(client,topic,msg):

    msg_format = '{\n'+ f"\t\"timestamp\" : \"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\",\n" + f"\t\"message\" : \"{msg}\"\n"+'}'
    client.publish(topic,msg_format)



def isHostUp(ip):
    
    with open('host_check','w') as f:
        proc = run(f'ping -c 1 {ip}',stdout=f,shell=True, encoding="utf-8", universal_newlines=True)
        
    with open('host_check','r') as f:
        f_data = f.readlines()
        for line in f_data:
            if '100% packet loss' in line:
                return False
        return True
    os.remove('host_check')



def get_VM_info(vm_index):
    
    with open(sys.argv[1],mode='r',encoding='utf-8') as f:
        data = yaml.full_load(f)
    f.close()
    return data['guest_ip'], data['login'], data['password']

def Create_SSH(guest_ip,vm_password,login):
    
    if None in [guest_ip,login,vm_password]:
        send_msg(client,'ssh/isHostUp/kpi/error/3','Missing required arguments')
        print('Missing one of the required credentails')
        exit()
        #return None
    
    if not isHostUp(guest_ip):
        send_msg(client,'ssh/isHostUp/kpi/error/3',f'Host {guest_ip} is Down or Not reachable')
        print(f'{guest_ip} is Not reachable')
        exit()
        #return None

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())  # Addedd to Keys if missing
    ssh.load_system_host_keys()
    
    try:
        ssh.connect(guest_ip, username=login, password=vm_password,allow_agent=False)
    except paramiko.ssh_exception.BadHostKeyException as e:
        os.system(f"ssh-keygen -f '/home/{getpass.getuser()}/.ssh/known_hosts' -R '{guest_ip}'")
        ssh = Create_SSH(guest_ip,vm_password,login)
    except Exception as e:
        print(f"Error: {e} while connecting to {guest_ip} as {login}")
    
    return ssh


def ssh_guest(cmd):

    guest_ip ,login, vm_password= get_VM_info(sys.argv[1])
    ssh = Create_SSH(guest_ip,vm_password,login)

    if ssh is None:
        send_msg(client,'ssh/isHostUp/kpi/error/3',f'Error in connecting to {guest_ip}')
        print('Error in creating connection')
        sys.exit()
    
    sftp = ssh.open_sftp()
    sftp.get(sys.argv[2],sys.argv[3])


ssh_guest(sys.argv)
