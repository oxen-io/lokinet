### lokinet android glue

additional requirements:

* gradle 6.x
* android sdk

building:

install `ndk-bundle` using android sdk manager tool:

    cd /path/to/android/sdk
    ./tools/bin/sdkmanager --install ndk-bundle

put into local.properties the following keys:

    sdk.dir=/path/to/android/sdk

run gradle and hope you don't run out for ram `:^)`

    gradle assemble
