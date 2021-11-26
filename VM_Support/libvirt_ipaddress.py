#!/usr/bin/env python3
#
# INTEL CONFIDENTIAL
#
# Copyright 2021 (c) Intel Corporation.
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them  is governed by the  express license under which  they were
# provided to you ("License"). Unless the License provides otherwise, you  may
# not  use,  modify,  copy, publish,  distribute,  disclose  or transmit  this
# software or the related documents without Intel"s prior written permission.
#
# This software and the related documents are provided as is, with no  express
# or implied  warranties, other  than those  that are  expressly stated in the
# License.
#
# ----------------------------------------------------------------------------


import sys
import libvirt

def Guest_IPAddress(dom):
    ifaces = dom.interfaceAddresses(libvirt.VIR_DOMAIN_INTERFACE_ADDRESSES_SRC_AGENT, 0)
#    print("The interface IP addresses:")
    for (name, val) in ifaces.items():
        if val['addrs']:
            for ipaddr in val['addrs']:
                if ipaddr['type'] == libvirt.VIR_IP_ADDR_TYPE_IPV4:
#                    print(ipaddr['addr'] + " VIR_IP_ADDR_TYPE_IPV4")
                    temp = ipaddr['addr'].split('.')
                    for i in temp:
                        if(i == '192'):
                            ipaddress = ipaddr['addr']
                            print("\n\r"+ipaddress)

    
    return ipaddress
