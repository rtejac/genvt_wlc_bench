import paramiko
import sys
import yaml
from scp import SCPClient
f =open(f"vm{sys.argv[1]}_ipaddress.yml",mode='r',encoding = 'utf-8') 
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
