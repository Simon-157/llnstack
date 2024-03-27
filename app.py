from scapy.all import *

# Craft ICMP Echo Request packet with custom message
packet = IP(dst="192.0.2.2") / ICMP() / "Hey this is my network stack"

# Send packet
send(packet)
