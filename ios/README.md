To build, run:

`cmake -G Xcode -DCMAKE_TOOLCHAIN_FILE=ios/ios.toolchain.cmake -DDEPLOYMENT_TARGET=10.0 -B ios/Core -S .`

followed by:

`cmake --build ios/Core --config Debug --target lokinet`

from the Loki Network root directory.

