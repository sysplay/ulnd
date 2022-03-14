#!/bin/bash

IFNAME=nic

if [ "$1" != "down" ]
then
	# Load the NIC
	insmod ${IFNAME}.ko

	# Disable IPv6 on NIC
	echo 1 > /proc/sys/net/ipv6/conf/${IFNAME}/disable_ipv6

	ip addr add 192.168.65.1/24 broadcast 192.168.65.255 dev ${IFNAME}
	ip link set ${IFNAME} up
else
	ip link set ${IFNAME} down
	ip addr del 192.168.65.1/24 broadcast 192.168.65.255 dev ${IFNAME}

	# Unload the NIC
	rmmod ${IFNAME}
fi
