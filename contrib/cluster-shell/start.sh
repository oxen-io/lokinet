#!/bin/bash
set +x
cd loki1
nohup ./lokinet1 $PWD/lokinet.ini &
# seed node needs some time to write RC to make sure it's not expired on load for the rest
sleep 1
cd ../loki2
nohup ./lokinet2 $PWD/lokinet.ini &
cd ../loki3
nohup ./lokinet3 $PWD/lokinet.ini &
cd ../loki4
nohup ./lokinet4 $PWD/lokinet.ini &
cd ../loki5
nohup ./lokinet5 $PWD/lokinet.ini &
cd ..
tail -f loki*/nohup.out
