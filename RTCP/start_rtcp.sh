python3 ~/Teja/msg_bus.py rtcp/PID/kpi/status connected
python3 SSH/detailed_genvt_ssh.py 2 'cd /usr/share/pnp-rtcp-xdp; ssh root@10.158.77.20 "bash -c \"(cd /usr/share/pnp-rtcp-xdp/ && sleep 3 && ./escripts/xdp5a.sh enp0s4f0 2400000 64 125000 100000 0 > /var/log/rtcp.log 2>&1) &\" > /dev/null" & ./escripts/xdp4b.sh enp0s4f0 2400000 64 125000 0x6E000 0x1B8000'
