#!/bin/sh

# test36: Create a tun1 persistent device and release it

TEST="`pwd`/helper36"
SYSTEM=`uname`
TARGET='tun1'
TYPE='tun'

if [ "$SYSTEM" = "Linux" ]; then
	IFDEL="ip tuntap del $TARGET mode $TYPE"
else
	IFDEL="ifconfig $TARGET destroy"
fi

OK=0
$TEST && OK=1

# If the $TEST was a success, check if the interface still exist
if [ $OK -eq 1 ]; then
	ifconfig $TARGET && OK=2
else
	exit 1
fi

# The $TARGET still exists, clean it and exit success
if [ $OK -eq 2 ]; then
	$IFDEL
	exit 0
fi

exit 1
