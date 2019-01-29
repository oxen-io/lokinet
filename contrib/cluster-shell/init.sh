#!/bin/sh
# generate default config file
../../lokinet -g lokinet.ini
# make seed node
./makenode.sh 1
# establish bootstrap
ln -s loki1/self.signed bootstrap.signed
