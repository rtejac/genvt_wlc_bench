#!/bin/bash
# start ptp4l, phc2sys in PG
ssh -o "UserKnownHostsFile=/dev/null" -o "StrictHostKeyChecking=no" root@rtcp-pg1 "bash -c 'cd /home/kpi/RT-KPI/XDP/stmmac-xdp-internal/ && ./escripts/setup-xdp5a.sh eth0 > /var/log/pg.log 2>&1'"
# start ptp4l, phc2sys and rtcp in SUT
ssh -o "UserKnownHostsFile=/dev/null" -o "StrictHostKeyChecking=no" root@rtcp-sut1 "bash -c 'cd /home/kpi/RT-KPI/tsn_ref_sw_linux_adl && ./run.sh adl enp0s4f0 vs1b run > /var/log/rtcp.log 2>&1'" &
# wait until rtcp application starts
sleep 10
# send mqtt connection message
ssh -o "UserKnownHostsFile=/dev/null" -o "StrictHostKeyChecking=no" root@rtcp-sut1 /home/kpi/RT-KPI/tsn_ref_sw_linux_adl/rtcp_monitor.sh &
# wait until rtcp transmission starts
sleep 45
# start rtcp in PG
ssh -o "UserKnownHostsFile=/dev/null" -o "StrictHostKeyChecking=no" root@rtcp-pg1 "bash -c 'cd /home/kpi/RT-KPI/XDP/stmmac-xdp-internal && ./escripts/xdp5a.sh eth0 300000 64 1000000 100000 0 >> /var/log/pg.log 2>&1'"
# read out log for the sake of wlc-bench
ssh -o "UserKnownHostsFile=/dev/null" -o "StrictHostKeyChecking=no" root@rtcp-sut1 cat /var/log/rtcp.log
# wait until wlc-bench close me
sleep infinity
