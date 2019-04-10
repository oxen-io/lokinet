There are a few steps that need to be taken to get the Loki Network C++ library to interoperate with Swift. They're a bit tricky to figure out, so for future reference:

- Run `make -j8` from the loki-network root folder
- Drag build/vendor/abseil-cpp/absl/base/libabsl_internal_throw_delegate.a, build/vendor/abseil-cpp/absl/strings/libabsl_strings.a, build/vendor/abseil-cpp/absl/hash/libabsl_hash.a, build/vendor/abseil-cpp/absl/hash/libabsl_internal_city.a, build/vendor/libcppbackport.a, build/llarp/liblokinet-util.a, build/llarp/liblokinet-platform.a and build/llarp/liblokinet-static.a into the project and make sure they're included under Build Phases → Link Binary With Libraries
- Update Build Settings → **Library** Search Paths to include `$(PROJECT_DIR)/../build/vendor/abseil-cpp/absl/base`, `$(PROJECT_DIR)/../build/vendor/abseil-cpp/absl/hash`, `$(PROJECT_DIR)/../build/vendor/abseil-cpp/absl/strings`, `$(PROJECT_DIR)/../build/vendor` and `$(PROJECT_DIR)/../build/llarp`
- Drag include/llarp.h into the project 
- Update Build Settings → **System** Header Search Paths to include `$(PROJECT_DIR)/../include`
- Create a liblokinet-wrapper.cpp, include llarp.h, and wrap the functions you want to expose
- Create a liblokinet-wrapper.h and include any imports needed for the function definitions
- Create a Loki-Network-Bridging-Header.h and include liblokinet-wrapper.h
- Set Build Settings → Objective-C Bridging Header to Loki-Network-Bridging-Header.h
- Update Build Settings → **System** Header Search Paths to include `$(PROJECT_DIR)/../` ... `llarp`, `crypto/include`, `vendor/abseil-cpp` and `vendor/nlohmann/include`
