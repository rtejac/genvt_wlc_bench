import sys
import yaml
import paramiko


def get_VM_info():
    
    with open('ipaddress.yml',mode='r',encoding='utf-8') as f:
        data = yaml.full_load(f)
    f.close()
    return data['guest_ip'], data['login'], data['password']


def Create_SSH():
    
    if None in [host,un,pwd]:
        print('Missing one of the required credentails')
        return None

    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())  # Addedd to Keys if missing
    ssh.load_system_host_keys()
    ssh.connect(host, username=un, password=pwd)

    return ssh


host, un, pwd = get_VM_info()
ssh = Create_SSH()

if ssh is None:
    print('Error in creating connection')
    sys.exit()



process = sys.argv[1]
ssh.exec_command(f'kill -9 $(pgrep {process})')
