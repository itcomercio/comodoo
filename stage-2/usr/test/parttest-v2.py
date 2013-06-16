#!/usr/bin/python
#
# Note for sfdisk warnings:
#
# Modern linux and operating systems don't use 
# CHS (cylinder,head, sector) adressing anymore. 
# They rely on LBA (linear block addressing) that 
# is handled by the bios and the disk drivers.
# The reported sfdisk warnings may be safely ignored.

import parted, sys

MEGABYTE = 1024 * 1024

if len(sys.argv) <= 1:
	print "You must call this script with a device name"
	sys.exit(1)

def megabytes_to_sectors(mb, sector_bytes=512):
	return long(mb * MEGABYTE / sector_bytes)

def sectors_to_megabytes(sectors, sector_bytes=512):
	return float((float(sectors) * sector_bytes)/ float(MEGABYTE))

def convert_bytes(bytes):
    bytes = float(bytes)
    if bytes >= 1099511627776:
        terabytes = bytes / 1099511627776
        size = '%.2fT' % terabytes
    elif bytes >= 1073741824:
        gigabytes = bytes / 1073741824
        size = '%.2fG' % gigabytes
    elif bytes >= 1048576:
        megabytes = bytes / 1048576
        size = '%.2fM' % megabytes
    elif bytes >= 1024:
        kilobytes = bytes / 1024
        size = '%.2fK' % kilobytes
    else:
        size = '%.2fb' % bytes
    return size

# Create Device object
target_device = parted.Device(path="/dev/sdb")

(cylinders, heads, sectors) = target_device.biosGeometry
sizeInBytes = target_device.length * target_device.sectorSize
sys.stdout.write("%d heads, %d sectors/track, %d cylinders (sector bytes: %d)\n" \
			% (heads, sectors, cylinders, target_device.sectorSize))
sys.stdout.write("Disk /dev/sdb size: %s\n" % (convert_bytes(sizeInBytes),))

# Create Disk object
target_disk = parted.freshDisk(target_device, "msdos")

# Create Constraint object
target_constraint = parted.Constraint(device = target_device)

#
# Geometry represents a region on a device in the system - a disk or
# partition.  It is expressed in terms of a starting sector and a length.
#
# Geometry Class : Create a new Geometry object for the given _ped.Device 
# that extends for length sectors from the start sector.  
# Optionally, an end sector can also be provided.
#

# Create geometry for 100MB from sector 1 - boot partition
bootsize = megabytes_to_sectors(100)

boot_partition_geom = parted.Geometry(device=target_device, start=1, end=bootsize) 
filesystem_target = parted.FileSystem(type="ext3", geometry=boot_partition_geom)
boot_partition = parted.Partition(disk=target_disk, fs=filesystem_target,
        type=parted.PARTITION_NORMAL, geometry=boot_partition_geom)

# Create geometry for 500MB of swap partition
swapsize = megabytes_to_sectors(500)

swap_partition_geom = parted.Geometry(device=target_device, start=bootsize+1, end=bootsize+swapsize)
filesystem_target = parted.FileSystem(type="linux-swap(v1)", geometry=swap_partition_geom)
swap_partition = parted.Partition(disk=target_disk, fs=filesystem_target,
        type=parted.PARTITION_NORMAL, geometry=swap_partition_geom)

root_partition_geom = parted.Geometry(device=target_device, start=bootsize+swapsize+1, end=target_device.length-1)
filesystem_target = parted.FileSystem(type="ext3", geometry=root_partition_geom)
root_partition = parted.Partition(disk=target_disk, fs=filesystem_target,
        type=parted.PARTITION_NORMAL, geometry=root_partition_geom)


# Delete all partitions in the drive
target_disk.deleteAllPartitions()
# Add new partitions
target_disk.addPartition(partition = boot_partition, constraint=target_constraint)
target_disk.addPartition(partition = swap_partition, constraint=target_constraint)
target_disk.addPartition(partition = root_partition, constraint=target_constraint)
# All the stuff we just did needs to be committed to the disk.
target_disk.commit()
