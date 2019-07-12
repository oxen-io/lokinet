To compile libuv for arm64, download libuv from https://github.com/libuv/libuv/, copy ios-configure-arm64 to its root folder, and execute:

```
brew install autoconf (if you don't have it installed already)
bash autogen.sh
chmod +x ios-configure-arm64
./ios-configure-arm64
```

