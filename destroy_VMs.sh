for ((i=0;i<$1;i++)); 
do 
   # your-unix-command-here
    echo $i
    virsh destroy ubuntu-vm-$i
    virsh undefine ubuntu-vm-$i
    #rm vm_$i.xml
done

