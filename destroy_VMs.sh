#!/bin/bash
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


if [ $1 == 'all' ]
then


for ((i=0;i<$2;i++)); 
do 
    virsh destroy ubuntu-vm-$i
    virsh undefine ubuntu-vm-$i
    echo VM $i deleted

done

else
    virsh destroy ubuntu-vm-$i
    virsh undefine ubuntu-vm-$i
    echo VM $i deleted
fi
