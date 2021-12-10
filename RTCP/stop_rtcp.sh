#!/bin/bash

python3 SSH/SSH_exec_from_file.py 2 RTCP/stop_rtcp.txt
python3 SSH/detailed_genvt_ssh.py 2 'cd /usr/share/pnp-rtcp-xdp/; escripts/xdp4b-calc.sh' 'rtcp'

python3 RTCP/run_stats.py logs/rtcp.log
