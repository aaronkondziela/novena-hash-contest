#!/usr/bin/env bash
while [ 1 == 1 ] ; do
	seq 22 | parallel -n0 -u --gnu ./novena -d 107 >> novena.out
done
