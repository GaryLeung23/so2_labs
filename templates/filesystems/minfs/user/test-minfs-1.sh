#!/bin/sh

# load module
insmod ../kernel/minfs.ko

# create mount point
mkdir -p /mnt/minfs

# format partition
./mkfs.minfs /dev/vdd

# mount filesystem
mount -t minfs /dev/vdd /mnt/minfs

# list all filesystem files
cd /mnt/minfs
ls -lai

# unmount filesystem
cd ..
umount /mnt/minfs

# unload module
rmmod minfs
