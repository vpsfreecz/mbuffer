#!/bin/bash
rm -f output.$0
cat ../mbuffer | LD_PRELOAD=./tapetest.so ../mbuffer -s10k -f -o output.$0 -H -A "echo '[$0] Replacing tape'" --tapeaware
cat output.$0* | openssl md5 > $0.md5
rm -f output.$0*
$DIFF $0.md5 mbuffer.md5
