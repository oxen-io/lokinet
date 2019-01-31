mkdir loki$1
cd loki$1
ln -s ../lokinet lokinet$1
cp ../lokinet.ini .
nano lokinet.ini
cd ..
echo "killall -9 lokinet$1" >> ../stop.sh
