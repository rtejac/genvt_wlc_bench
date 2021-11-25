for ((i=0;i<$1;i++)); 
do 
    virsh destroy ubuntu-vm-$i
    virsh undefine ubuntu-vm-$i
    echo VM $i deleted
done

