#!/bin/bash

rm -rf build && \
mkdir build && \
cd build && \
cmake .. -DCMAKE_BUILD_TYPE=Debug -DWITH_EMBEDDED_LOKINET=ON -DLIBLOKINET_TEST_UTILS=ON -DOXEN_LOGGING_RELEASE_TRACE=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=1 && \
make -j6 && \
mkdir -p tcp_connect_data_dir/testnet && \
cp ../contrib/bootstrap/mainnet.signed ./tcp_connect_data_dir/bootstrap.signed && \
cp ../contrib/bootstrap/testnet.signed ./tcp_connect_data_dir/testnet/bootstrap.signed && \
sudo setcap cap_net_admin,cap_net_bind_service=+eip daemon/lokinet
