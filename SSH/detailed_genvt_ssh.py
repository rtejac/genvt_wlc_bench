import os
import sys
import yaml
import paramiko


def get_VM_info(vm_index):
    
    with open(f'VM_Files/vm{vm_index}_ipaddress.yml',mode='r',encoding='utf-8') as f:
        data = yaml.full_load(f)
    f.close()
    return data['guest_ip'], data['login'], data['password']

def Create_SSH(guest_ip,vm_password,login):
    
    if None in [guest_ip,login,vm_password]:
        print('Missing one of the required credentails')
        return None

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())  # Addedd to Keys if missing
    ssh.load_system_host_keys()
    
    try:
        ssh.connect(guest_ip, username=login, password=vm_password,allow_agent=False)
        print(f'\rConnected to {guest_ip} as {login}')
    except paramiko.ssh_exception.BadHostKeyException as e:
        os.system(f"ssh-keygen -f '/home/wlc/.ssh/known_hosts' -R '{guest_ip}'")
        ssh = Create_SSH(guest_ip,vm_password,login)
    except Exception as e:
        print(f"Error: {e} while connecting to {guest_ip} as {login}")
    
    return ssh


def ssh_guest(cmd):

    guest_ip ,login, vm_password= get_VM_info(sys.argv[1])
    ssh = Create_SSH(guest_ip,vm_password,login)

    if ssh is None:
        print('Error in creating connection')
        sys.exit()

    print(f"\r\nExecuting the WL by the command :{cmd[2]}")
    #cmd = ' '.join(sys.argv[2:])
    try:
        stdin, stdout, stderr = ssh.exec_command(sys.argv[2],get_pty=True)
    except:
        #Kill the process directly and print the output generated so far(???)
        pass
    lines = stdout.readlines()
    error = stderr.readlines()
    
    if len(sys.argv) == 4:
        with open(f'logs/{sys.argv[3]}.log','w') as f:
            for line in lines:
                f.write(line)
    else:
        for line in lines:
            print('\r',line)
        for data in error:
            print(f"\n\r{data}")


ssh_guest(sys.argv)
