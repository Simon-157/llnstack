#!/bin/bash

# Check for required arguments
if [ $# -ne 2 ]; then
  echo "Usage: $0 <server_ip> <server_port>"
  exit 1
fi

# Assign arguments to variables
server_ip="$1"
server_port="$2"

# Run make command to compile the server binary
make

# Create TAP interface
sudo ip tuntap add mode tap user $USER name tap0

# Set IP address for TAP interface
sudo ip addr add 192.0.2.1/24 dev tap0

# Bring TAP interface up
sudo ip link set tap0 up

# Navigate to server binary directory
cd bin

# Run server with provided IP and port
./server "$server_ip" "$server_port"

# Remove TAP interface
trap 'sudo ip tuntap del mode tap name tap0' EXIT
