#!/bin/sh

# test35: Create a tap0 persistent device and release it

TEST="`pwd`/helper35"
SYSTEM=`uname`

# There is no tap driver on OpenBSD
if [ "$SYSTEM" = "OpenBSD" ]; then
	TARGET='tun0'
	TYPE='tap'
else
	TARGET='tap0'
	TYPE='tap'
fi

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
	exit 0
else
	exit 1
fi

