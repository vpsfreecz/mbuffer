#!/bin/bash
TESTFILE=${TESTFILE:-$1}
if [ ! -r "$TESTFILE" ]; then
	echo TESTFILE is not set, not found, or inaccessible
	exit 1
fi
../mbuffer -i $TESTFILE -f -o $0.tar -o /dev/null -H || exit 1
cmp $0.tar $TESTFILE || exit 1
rm -f $0.tar
