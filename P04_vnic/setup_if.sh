#!/bin/bash

IFNAME=eth0

if [ "$1" != "down" ]
then
	insmod packeted_network_driver.ko

	# Disable IPv6 on new interface
	echo 1 > /proc/sys/net/ipv6/conf/${IFNAME}/disable_ipv6

	ip addr add 192.168.64.1/24 broadcast 192.168.64.255 dev ${IFNAME}
	ip link set ${IFNAME} up
else
	ip link set ${IFNAME} down
	ip addr del 192.168.64.1/24 broadcast 192.168.64.255 dev ${IFNAME}

	rmmod packeted_network_driver
fi
