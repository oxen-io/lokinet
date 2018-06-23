#!/bin/sh

# test34: Create a tun0 persistent device and destroy it

TEST="`pwd`/helper34"
SYSTEM=`uname`
TARGET='tun0'
TYPE='tun'

if [ "$SYSTEM" = "Linux" ]; then
	IFDEL="ip tuntap del $TARGET mode $TYPE"
else
	IFDEL="ifconfig $TARGET destroy"
fi

OK=0
$TEST && OK=1

# The $TEST is successful
if [ $OK -eq 1 ]; then
	ifconfig $TARGET && OK=2
else
	exit 1
fi

# The $TARGET still exists
if [ $OK -eq 2 ]; then
	$IFDEL
	exit 1
fi

exit 0
