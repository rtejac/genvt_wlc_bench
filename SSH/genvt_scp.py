import paramiko
import sys
import yaml
import getpass
from scp import SCPClient
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
                send_msg(client,'ssh/isHostUP/kpi/error/3',f'{ip} is not reachable')
                return False
        return True
    os.remove('host_check')


host_user = getpass.getuser()
f =open(f"VM_Files/vm{sys.argv[1]}_ipaddress.yml",mode='r',encoding = 'utf-8') 
read_data = yaml.full_load(f)
isHostUp(read_data['guest_ip'])
ssh = paramiko.SSHClient()
ssh.load_host_keys(f'/home/{host_user}/.ssh/known_hosts')
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
