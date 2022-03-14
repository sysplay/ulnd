#!/bin/bash

if [ "$1" != "down" ]
then
	sudo ./nic.sh
	sudo ./setup_if.sh
	#sudo arp -s 192.168.64.2 00:56:4e:49:43:53
	#sudo arp -s 192.168.65.2 00:01:02:03:04:05
else
	#sudo arp -d 192.168.65.2
	#sudo arp -d 192.168.64.2
	sudo ./setup_if.sh down
	sudo ./nic.sh down
fi
