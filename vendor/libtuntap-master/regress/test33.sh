#!/bin/sh

# test33: Create a tap0 persistent device and destroy it

TEST="`pwd`/helper33"
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

# If the $TEST was a success, check if the interface still exist
if [ $OK -eq 1 ]; then
	ifconfig $TARGET > /dev/null && OK=2
else
	exit 1
fi

# The $TARGET still exists, clean it and return failure
if [ $OK -eq 2 ]; then
	$IFDEL
	exit 1
fi

# Everything went fine
exit 0
