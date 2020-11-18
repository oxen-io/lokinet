#!/bin/bash
set -x
test `whoami` == root || exit 1

# this is for dns tomfoolery
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

# get guid for service
SERVICE_GUID=`scutil_query State:/Network/Global/IPv4 \
        | grep "PrimaryService" \
        | awk '{print $3}'`

# get name of network service
SERVICE_NAME=`scutil_query Setup:/Network/Service/$SERVICE_GUID \
        | grep "UserDefinedName" \
        | awk -F': ' '{print $2}'`

# tell dns to be "empty" so that it's reset
networksetup -setdnsservers "$SERVICE_NAME" empty

# shut off exit if it's up
pgrep /opt/lokinet/bin/lokinet && /opt/lokinet/bin/lokinet-vpn --down

 # Prevent restarting on exit
touch /var/lib/lokinet/suspend-launchd-service
# kill the gui and such
killall LokinetGUI
killall lokinet
# if the launch daemon is there kill it
/bin/launchctl stop network.loki.lokinet.daemon
/bin/launchctl unload /Library/LaunchDaemons/network.loki.lokinet.daemon.plist

# kill it and make sure it's dead 
killall -9 lokinet

rm -rf /Library/LaunchDaemons/network.loki.lokinet.daemon.plist
rm -rf /Applications/Lokinet/
rm -rf /Applications/LokinetGUI.app
rm -rf /var/lib/lokinet
rm -rf /usr/local/lokinet/
rm -rf /opt/lokinet

