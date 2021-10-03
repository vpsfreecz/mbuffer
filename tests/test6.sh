#!/bin/bash
LD_PRELOAD=../idev.so BSIZE=317 IDEV=../mbuffer ../mbuffer -s256 -i ../mbuffer -f -o /dev/null
