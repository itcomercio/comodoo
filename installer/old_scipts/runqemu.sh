# yum install bridge-utils

brctl addbr br0
ifconfig eth0 0.0.0.0 promisc up
brctl addif br0 eth0
dhclient br0
dhclient br0 -H testqemu
iptables -F FORWARD

# Xephyr :1 -ac -2button -host-cursor -screen 800x480

qemu -hda c.img -net nic,model=ne2k_pci,vlan=0 -net tap,vlan=0,ifname=tap0,script=./qemu-ifup

