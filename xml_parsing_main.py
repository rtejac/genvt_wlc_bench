import xml.etree.ElementTree as ET

def vm_xml_create(num,vm_name,os_image,gpu_pass,ram,cpu):

    mytree= ET.parse('VM_Files/vm_standered.xml')
    myroot = mytree.getroot()
    for name in myroot.iter('name'):
        a = str(vm_name)
        name.text = str(a)

    for mem in myroot.iter('memory'):
        a = str(ram)
        mem.text = str(a)
    for cmem in myroot.iter('currentMemory'):
        a = str(ram)
        cmem.text = str(a)
    for cpus in myroot.iter('vcpu'):
        a = str(cpu)
        cpus.text = str(a)
    for device_element in myroot.findall('devices'):
        if gpu_pass == 0:
            for ele in device_element.findall('hostdev'):
                device_element.remove(ele)
        for disk_element in device_element.findall('disk'):
            file_dict = disk_element.find('source').attrib
            file_dict['file'] = str(os_image)
            target_dict = disk_element.find('target').attrib
            target_dict['dev'] = str(f'vd{chr(vm_xml_create.lc_ch)}')
            vm_xml_create.lc_ch += 1
        for serial_element in device_element.findall('serial'):
            target_serial_dict = serial_element.find('target').attrib
            target_serial_dict['port'] = chr((num%10)+48)
        for console_element in device_element.findall('console'):
            target_console_dict = console_element.find('target').attrib
            target_console_dict['port'] = chr((num%10)+48)
            if num > 9:
                target_console_dict['type'] = 'virtio'
            else:
                target_console_dict['type'] = 'serial'

    mytree.write(f'VM_Files/vm_{num}.xml')	

# ascii value for 'a'
vm_xml_create.lc_ch = 97

