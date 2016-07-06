#!/bin/bash

# This runs on the rotobox and forwards the GPS over TCP port 54321
socat tcp-l:54321,reuseaddr,fork file:/dev/ttyS1,nonblock,raw,echo=0

# Then on Windows, open u-center and 'Receiver' -> 'Port' -> 'Network Connection'
# Use 'tcp://<ip_of_rotobox>:54321'