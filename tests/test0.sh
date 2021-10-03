#!/bin/bash
TESTFILE=${TESTFILE:-$1}
if [ ! -r "$TESTFILE" ]; then
	echo TESTFILE is not set, not found, or inaccessible
	exit 1
fi
../mbuffer -i $TESTFILE -p10 | ../mbuffer -q -P 90 | $OPENSSL md5 > $0.md5 || exit 1
$OPENSSL md5 < $TESTFILE > $TESTFILE.md5
$DIFF $0.md5 $TESTFILE.md5
