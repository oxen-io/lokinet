#!/bin/sh

touch /var/lib/lokinet/suspend-launchd-service # Prevent restarting on exit
killall lokinet
sleep 5  # Give it some time to shut down before we bring launchd into this
launchctl stop network.loki.lokinet.daemon
launchctl unload /Library/LaunchDaemons/network.loki.lokinet.daemon.plist


rm -rf /Library/LaunchDaemons/network.loki.lokinet.daemon.plist
rm -rf /Applications/LokinetGUI.app
rm -rf /var/lib/lokinet
rm -rf /usr/local/lokinet/
rm -rf /opt/lokinet

