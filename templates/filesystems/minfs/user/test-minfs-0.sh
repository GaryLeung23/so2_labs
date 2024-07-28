#!/bin/sh

# load module
insmod ../kernel/minfs.ko

# create mount point
mkdir -p /mnt/minfs

# format partition
./mkfs.minfs /dev/vdd

# mount filesystem
mount -t minfs /dev/vdd /mnt/minfs

# show registered filesystems
cat /proc/filesystems

# show mounted filesystems
cat /proc/mounts

# umount filesystem
umount /mnt/minfs

# unload module
rmmod minfs
