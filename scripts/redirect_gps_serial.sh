#!/bin/bash

# This runs on the rotobox and forwards the GPS over TCP port 54321.
# Jump over to a Windows machine, open u-center: 'Receiver' -> 'Port' -> 'Network Connection'
# Use 'tcp://<ip_of_rotobox>:54321'

ips=$(ifconfig | awk '/inet addr/{print substr($2,6)}')

echo "Redirecting ttyS1 (GPS):"
for ip in $ips
do
	echo "  tcp://$ip:54321"
done
echo ""
echo "Press Ctrl-C to stop..."

socat tcp-l:54321,reuseaddr,fork file:/dev/ttyS1,nonblock,raw,echo=0
