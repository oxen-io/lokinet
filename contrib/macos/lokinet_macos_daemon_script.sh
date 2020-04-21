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

networksetup -setdnsservers "$SERVICE_NAME" 127.0.0.1

/opt/lokinet/bin/lokinet /var/lib/lokinet/lokinet.ini

networksetup -setdnsservers "$SERVICE_NAME" $OLD_SERVERS

