#!/bin/bash
sudo qemu-system-x86_64 \
    -boot c \
    -m 128 \
    -object memory-backend-file,id=mem,size=128M,mem-path=/dev/hugepages/vhost,share=on \
    -numa node,memdev=mem \
    -mem-prealloc \
    -chardev socket,id=chr0,path=/home/kaifeng/my-vapp/vhost.sock \
    -netdev type=vhost-user,id=net0,chardev=chr0 \
    -device virtio-net-pci,netdev=net0

