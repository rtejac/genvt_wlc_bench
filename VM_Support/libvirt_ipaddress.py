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
