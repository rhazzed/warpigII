#!/usr/bin/env python

from scapy.all import *
import sys
import subprocess

dst_ip="111.222.111.222"

data= "  *** NOT FROM BALLOON *** - FLUSH all packets, upon command from Hulk - Frequency keyword included"

for src_prt in 53,2947:
	for dst_prt in 53,80,1053,2947:
		print("Sending s:%d d:%d\n" % (src_prt,dst_prt))
		packet = IP(dst=dst_ip)/UDP(dport=dst_prt, sport=src_prt)/data
		send(packet,verbose=False) 
quit()

