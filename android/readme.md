# lokinet android

this directory contains basic stuff for lokinet on android.

## Prerequsites

To build you need the following:

* Gradle (6.x)
* Android SDK (latest version)
* Android NDK (latest version)

## Building

Next set up the path to Android SDK and NDK in `local.properties`

```
sdk.dir=/path/to/android/sdk
ndk.dir=/path/to/android/ndk
```

Then build:

    $ gradle assemble

This fetches a large amount (several dozen Gigabytes) of files from some 
server somewhere dumping it on your filesystem to make the thing do the 
building, then proceeds to peg all your cores for several dozen minutes
while it does the required incantations to build 2 apks.

The build outputs apks to to subdirectories in `build/outputs/apk/`
one called `debug` for debug builds and one called `release` for release builds.

