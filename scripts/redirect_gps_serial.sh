#!/bin/bash

socat tcp-l:54321,reuseaddr,fork file:/dev/ttyS1,nonblock