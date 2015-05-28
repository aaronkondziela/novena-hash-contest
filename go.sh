#!/usr/bin/env bash

IP=`ifconfig | grep eth0 -A1 | tail -n1 | perl -ne 'm/addr:([^ ]+)/; print "$1"'`

# gen hashes
echo "STATUS: Running gen $IP" | /home/aaron/sendlines.sh status

/home/aaron/gen > /home/aaron/h

echo "STATUS: Starting hashcat $IP" | /home/aaron/sendlines.sh status

cd /home/aaron/cudaHashcat-1.20
nohup ./cudaHashcat64.bin -m 0 -a 3 /home/aaron/h ?a?a?a?a?a?a?a?a &

echo "STATUS: Running $IP" | /home/aaron/sendlines.sh status

tail -f cudaHashcat.pot | /home/aaron/sendlines.sh hash

