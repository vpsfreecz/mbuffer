#!/bin/bash
TESTFILE=${TESTFILE:-$1}
if [ ! -r "$TESTFILE" ]; then
	echo TESTFILE is not set, not found, or inaccessible
	exit 1
fi
../have-af inet6
if [ "0" != "$?" ]; then
	echo 'SKIPPING the IPv6-only test!'
	exit
fi
rm -f $0.out0 $0.out1
../mbuffer --pid -q -6 -I :7002 -o $0.out0 -o - | $OPENSSL md5 > $0.md5 &
sleep 1
../mbuffer --pid -i "$TESTFILE" -o /dev/null -6 -O ::1:7002 -o $0.out1 -H || exit 1
wait || exit 1
cmp "$TESTFILE" "$0.out0" || exit 1
cmp "$TESTFILE" "$0.out1" || exit 1
$DIFF $0.md5 test.md5 || exit 1
rm "$0.md5" "$0.out0" "$0.out1"
