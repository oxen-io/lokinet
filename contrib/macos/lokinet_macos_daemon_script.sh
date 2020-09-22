#!/bin/sh

scutil_query()
{
  key=$1

  scutil<<EOT
  open
  get $key
  d.show
  close
EOT
}

SERVICE_GUID=`scutil_query State:/Network/Global/IPv4 \
        | grep "PrimaryService" \
        | awk '{print $3}'`

SERVICE_NAME=`scutil_query Setup:/Network/Service/$SERVICE_GUID \
        | grep "UserDefinedName" \
        | awk -F': ' '{print $2}'`

OLD_SERVERS="$(networksetup -getdnsservers "$SERVICE_NAME" \
        | tr '\n' ' ' \
        | sed 's/ $//')"

# <3 Apple
# 
# if there were no explicit DNS servers, this will return:
# "There aren't any DNS Servers set on Ethernet."
# This might be internationalized, so we'll suffice it to see if there's a space
pattern=" |'"
if [[ $OLD_SERVERS =~ $pattern ]]
then
  # and when there aren't any explicit servers set, we want to pass the literal
  # string "empty"
  OLD_SERVERS="empty"
fi

networksetup -setdnsservers "$SERVICE_NAME" 127.0.0.1

trap "networksetup -setdnsservers \"$SERVICE_NAME\" $OLD_SERVERS" INT TERM EXIT

/opt/lokinet/bin/lokinet /var/lib/lokinet/lokinet.ini

