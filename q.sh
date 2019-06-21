#!/bin/bash
sudo qemu-system-x86_64 \
    -enable-kvm \
    -m 512 \
    -object memory-backend-file,id=mem,size=512M,mem-path=/dev/hugepages/vhost,share=on \
    -numa node,memdev=mem \
    -chardev socket,id=charnet0,path=/home/kaifeng/gh/my-vapp/vhost.sock \
    -netdev vhost-user,chardev=charnet0,id=hostnet0 \
    -device virtio-net-pci,netdev=hostnet0,id=net0

#    -chardev socket,id=chr0,path=/home/kaifeng/gh/my-vapp/vhost.sock \
