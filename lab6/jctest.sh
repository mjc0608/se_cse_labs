#!/bin/bash

for ((i=0; i<100; ++i))
do
    echo round $i
    ./start.sh 1>/dev/null 2>&1
    ./test-lab-5 yfs1 yfs2
    ./stop.sh 1>/dev/null 2>&1
    ./stop.sh 1>/dev/null 2>&1
    ./stop.sh 1>/dev/null 2>&1
    echo
    echo
    echo
done
