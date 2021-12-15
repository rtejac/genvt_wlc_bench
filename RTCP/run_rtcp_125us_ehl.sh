#!/bin/bash
# Disable host key checking by setting $HOME/.ssh/config as below
# Host *
#   StrictHostKeyChecking no
#   UserKnownHostsFile=/dev/null

# Start ptp4l, phc2sys in PG if they're not yet running
echo Start PTP in PG
ssh root@cbrd-ehl35 "bash -c 'pidof ptp4l || cd /usr/share/pnp-rtcp-xdp; ./escripts/setup-xdp5a.sh enp0s29f2 > /var/log/pg.log 2>&1 &'" > /dev/null 2>&1

# Start ptp4l, phc2sys and rtcp in SUT if they're not yet running
echo Start PTP in SUT
ssh root@cbrd-ehl37-rtvm "bash -c 'pidof ptp4l || cd /usr/share/pnp-rtcp-xdp; ./escripts/setup-xdp4b.sh enp0s4f0 > /var/log/sut.log 2>&1 &'" > /dev/null 2>&1

echo Wait for PG to be ready
ssh root@cbrd-ehl35 "tail --pid=\`ps aux | grep setup-xdp5a.sh | grep -v grep | awk '{print \$2}'\` -f /dev/null > /dev/null 2>&1" > /dev/null 2>&1

echo Wait for SUT to be ready
ssh root@cbrd-ehl37-rtvm "tail --pid=\`ps aux | grep setup-xdp4b.sh | grep -v grep awk | '{print \$2}'\` -f /dev/null > /dev/null 2>&1" > /dev/null 2>&1

echo Start 1ms 20K IPCP RTCP in SUT
ssh root@cbrd-ehl37-rtvm "bash -c 'cd /usr/share/pnp-rtcp-xdp; rm afxdp* trace*; ./escripts/xdp4b.sh enp0s4f0 600000 64 500000 0x1B800 0x6E000 > /var/log/rtcp.log 2>&1 &'" > /dev/null 2>&1

echo Start RTCP monitor in SUT
ssh root@cbrd-ehl37-rtvm "/usr/share/pnp-rtcp-xdp/rtcp_monitor.sh ehl 500us cbrd-ehl37.png.intel.com &" > /dev/null 2>&1

echo Start 1ms RTCP in PG
ssh root@cbrd-ehl35 "bash -c 'cd /usr/share/pnp-rtcp-xdp; ./escripts/xdp5a.sh enp0s29f2 600000 64 500000 100000 0 > /var/log/rtcp.log 2>&1 &'" > /dev/null 2>&1

echo Wait for RTCP completion
ssh root@cbrd-ehl37-rtvm "tail --pid=\`ps aux | grep xdp4b.sh | grep -v grep | awk '{print \$2}'\` -f /dev/null > /dev/null 2>&1; sync" > /dev/null 2>&1

echo Calculate RTCP results
ssh root@cbrd-ehl37-rtvm "bash -c 'cd /usr/share/pnp-rtcp-xdp; mv /dev/shm/* .; ./bin2txt afxdp-fwdtstamps.log > afxdp-fwdtstamps.txt; taskset -c 0,1 ./escripts/xdp4b-calc.sh'"

echo Wait for closure by WLC-Bench
sleep infinity
