#!/bin/bash
python3 genvt_scp.py 0 synbench '~/GenVT_Env/'
python3 genvt_scp.py 0 start_wl.sh '~/start_wl.sh'
python3 genvt_scp.py 0 init_wl.sh '~/init_wl.sh'
python3 genvt_ssh.py 0 'chmod +x ~/start_wl.sh'
python3 genvt_ssh.py 0 'chmod +x ~/init_wl.sh'
python3 genvt_ssh.py 0 'cd ~ && ./init_wl.sh' 'proxy_init_log.log'
