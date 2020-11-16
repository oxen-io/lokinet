#!/bin/sh

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

 # Prevent restarting on exit
[ -e /var/lib/lokinet ] && touch /var/lib/lokinet/suspend-launchd-service
# kill it
killall lokinet || true
# Give it some time to shut down before we bring launchd into this
sleep 2
# make sure it's dead 
killall -9 lokinet || true
# if the launch daemon is there kill it
[ -e /Library/LaunchDaemons/network.loki.lokinet.daemon.plist ] && (
    launchctl stop network.loki.lokinet.daemon ;
    launchctl unload /Library/LaunchDaemons/network.loki.lokinet.daemon.plist
)

rm -rf /Library/LaunchDaemons/network.loki.lokinet.daemon.plist
rm -rf /Applications/Lokinet/
rm -rf /Applications/LokinetGUI.app
rm -rf /var/lib/lokinet
rm -rf /usr/local/lokinet/
rm -rf /opt/lokinet

