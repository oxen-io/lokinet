To compile Lokinet for iOS, execute the following from the Lokinet root directory:

```
cmake -G Xcode -DCMAKE_TOOLCHAIN_FILE=ios/ios.toolchain.cmake -DDEPLOYMENT_TARGET=10.0 -B ios/Core -S .
cmake --build ios/Core --config Debug --target lokinet
```

You can safely ignore the code signing warning that's emitted.
