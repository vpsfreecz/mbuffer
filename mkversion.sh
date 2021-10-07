#!/bin/bash

VERSION_H=${1:-version.h}
NEWFILE=`mktemp -t version.h.XXXXXXXXXX`
SED=`which gsed 2> /dev/null`
if [ $? != 0 ]; then
	SED=`which sed`
fi

vinfo=0
if [ -f .hg_archival.txt ]; then
	HGA=.hg_archival.txt
	# Gather version information from .hg_archival.txt for archives.
	TAG=`awk '/^tag:/ {printf("%s",$2);}' $HGA`
	LTAG=`awk '/^latesttag:/ {printf("%s",$2);}' $HGA`
	TAGD=`awk '/^latesttagdistance:/ {printf("%s",$2);}' $HGA`
	if [ "" != "$TAG" ]; then
		VER=$TAG
	elif [ "" = "$LTAG" ]; then
		echo warning: archive has no tag or latesttag
		VER=""
	elif [ "0" != $TAGD ]; then
		VER=$LTAG.$TAGD
	else
		VER=$LTAG
	fi
	if [ "$VER" != "" ]; then
		echo "#define VERSION \"$VER\"" > $NEWFILE
		BR=`awk '/^branch:/ {printf("%s",$2);}' $HGA`
		echo "#define HG_BRANCH \"$BR\"" >> $NEWFILE
	else
		rm $NEWFILE
	fi
elif [ -d .hg ]; then
	HG=${HG:-`which hg`}
	if [ "$?" == "0" ]; then
		# Check if we have modified, removed, added or deleted files.
		if [ `$HG st -mard | wc -l` != "0" ]; then
			# add delta indicator
			DELTA="+"
		else
			DELTA=""
		fi
		# Gather version information from repository and sandbox.
		VER=`$HG id -T"{latesttag}{if(latesttagdistance,'.{latesttagdistance}')}" 2>/dev/null`
		if [ $? == 0 ]; then
			$HG log -r. -T"`cat version.t`" | $SED "s/\$DELTA/$DELTA/" > $NEWFILE
		else
			echo "warning: your mercurial version is too old"
			rm $NEWFILE
		fi
	else
		rm $NEWFILE
	fi
else
	# Bail out with an error, if no version information an be gathered.
	echo "warning: no version information available"
	rm $NEWFILE
fi

if [ ! -f $NEWFILE ]; then
	if [ ! -f version.h ]; then
		echo "unable to determine version"
		cp version.u version.h
	fi
	exit
fi

if [ ! -f version.h ]; then
	echo creating version.h
	mv $NEWFILE $VERSION_H
	$SED "s/@VERSION@/$VER/" < mbuffer.1.in > mbuffer.1
	exit
fi

cmp -s $VERSION_H $NEWFILE

if [ "0" = "$?" ]; then
	echo version.h is up-to-date
	rm $NEWFILE
else
	echo updating version.h
	$SED "s/@VERSION@/$VER/" < mbuffer.1.in > mbuffer.1
	mv -f $NEWFILE $VERSION_H
fi
