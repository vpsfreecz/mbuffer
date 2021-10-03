#!/bin/bash
rm -f $0.out.*
cat ../mbuffer | LD_PRELOAD=../tapetest.so ../mbuffer -s10k -f -o $0.out -H -A "echo '[$0] Replacing tape'"
cat $0.out* | openssl md5 > $0.md5
$OPENSSL md5 > mbuffer.md5 < ../mbuffer
rm -f $0.out*
$DIFF $0.md5 mbuffer.md5
