#!/bin/bash
# Disable host key checking by setting $HOME/.ssh/config as below
# Host *
#   StrictHostKeyChecking no
#   UserKnownHostsFile=/dev/null

PLAT=$1
LABEL=$2
SOS=$3
SUT=$4
SUTIF=$5
PG=$6
PGIF=$7
NUMPKTS=$8
INTERVAL=$9
BUFSZ=${10}
SPAN=${11}

# Start ptp4l, phc2sys in PG if they're not yet running
echo Start PTP in PG
ssh root@cbrd-${PG} "bash -c 'pidof ptp4l || cd /usr/share/pnp-rtcp-xdp; ./escripts/setup-xdp5a.sh ${PGIF} > /var/log/pg.log 2>&1 &'" > /dev/null 2>&1

# Start ptp4l, phc2sys and rtcp in SUT if they're not yet running
echo Start PTP in SUT
ssh root@cbrd-${SUT} "bash -c 'pidof ptp4l || cd /usr/share/pnp-rtcp-xdp; ./escripts/setup-xdp4b.sh ${SUTIF} > /var/log/sut.log 2>&1 &'" > /dev/null 2>&1

echo Wait for PG to be ready
ssh root@cbrd-${PG} "tail --pid=\`ps aux | grep setup-xdp5a.sh | grep -v grep | awk '{print \$2}'\` -f /dev/null > /dev/null 2>&1" > /dev/null 2>&1

echo Wait for SUT to be ready
ssh root@cbrd-${SUT} "tail --pid=\`ps aux | grep setup-xdp4b.sh | grep -v grep awk | '{print \$2}'\` -f /dev/null > /dev/null 2>&1" > /dev/null 2>&1

echo Start RTCP in SUT
ssh root@cbrd-${SUT} "bash -c 'cd /usr/share/pnp-rtcp-xdp; rm afxdp* trace*; ./escripts/xdp4b.sh ${SUTIF} ${NUMPKTS} 64 ${INTERVAL} ${BUFSZ} ${SPAN} > /var/log/rtcp.log 2>&1 &'" > /dev/null 2>&1

echo Start RTCP monitor in SUT
ssh root@cbrd-${SUT} "/usr/share/pnp-rtcp-xdp/rtcp_monitor.sh ${PLAT} ${LABEL} cbrd-${SOS}.png.intel.com &" > /dev/null 2>&1

echo Start RTCP in PG
ssh root@cbrd-${PG} "bash -c 'cd /usr/share/pnp-rtcp-xdp; ./escripts/xdp5a.sh ${PGIF} ${NUMPKTS} 64 ${INTERVAL} 100000 0 > /var/log/rtcp.log 2>&1 &'" > /dev/null 2>&1

echo Wait for RTCP completion
ssh root@cbrd-${SUT} "tail --pid=\`ps aux | grep xdp4b.sh | grep -v grep | awk '{print \$2}'\` -f /dev/null > /dev/null 2>&1; sync" > /dev/null 2>&1

echo Calculate RTCP results
ssh root@cbrd-${SUT} "bash -c 'cd /usr/share/pnp-rtcp-xdp; mv /dev/shm/* .; ./bin2txt afxdp-fwdtstamps.log > afxdp-fwdtstamps.txt; taskset -c 0,1 ./escripts/xdp4b-calc.sh'"

echo Wait for closure by WLC-Bench
sleep infinity
