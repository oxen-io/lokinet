#!/bin/sh
# copy a lokinet binary into this cluster
cp ../../lokinet .
# generate default config file
./lokinet -g -r lokinet.ini
# make seed node
./makenode.sh 1
# establish bootstrap
ln -s loki1/self.signed bootstrap.signed
