#!/bin/sh
launchctl stop network.loki.lokinet.daemon
launchctl unload /Library/LaunchDaemons/network.loki.lokinet.daemon.plist

killall lokinet

rm -rf /Library/LaunchDaemons/network.loki.lokinet.daemon.plist
rm -rf /Applications/LokinetGUI.app
rm -rf /var/lib/lokinet
rm -rf /usr/local/lokinet/
rm -rf /opt/lokinet

