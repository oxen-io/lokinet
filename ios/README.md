To compile Lokinet for iOS, execute the following from the Lokinet root folder:

```
cmake -G Xcode -DCMAKE_TOOLCHAIN_FILE=ios/ios.cmake -DDEPLOYMENT_TARGET=10.0 -B ios/Core -S .
cmake --build ios/Core --config Debug --target lokinet
```

You can safely ignore the code signing warning that's emitted.

If you run into issues with libuv not being found, make sure you have it installed by running `brew install libuv` and/or add `include_directories(/usr/local/include)` to cmake/unix.cmake.
