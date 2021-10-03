#!/bin/bash
TESTFILE=${TESTFILE:-$1}
if [ ! -r "$TESTFILE" ]; then
	echo TESTFILE is not set, not found, or inaccessible
	exit 1
fi
../have-af inet
if "0" != "$?"; then
	echo 'no IPv4 support; SKIPPING the IPv4-only test!'
	exit
fi
../mbuffer --pid -q -4 -I :7001 -o "$0.tar" -o - | openssl md5 > $0.md5 &
sleep 1
../mbuffer --pid -i "$TESTFILE" -o /dev/null -4 -O localhost:7001 -H || exit 1
wait || exit 1
cmp "$0.tar" "$TESTFILE" || exit 1
$OPENSSL md5 < "$TESTFILE" > test.md5
$OPENSSL md5 < $0.tar > $0.md5 
rm -f $0.tar
$DIFF $0.md5 test.md5 || exit 1
