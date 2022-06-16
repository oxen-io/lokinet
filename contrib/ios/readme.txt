our scientists have yet to reverse engineer the correct way to use the apple build process as dictated on the official apple documentation so we have a made a hack to make it work until that occurs.
the build process for embedded lokinet on iphone is as follows:

* obtain holy water, sprinkle onto keyboard and single button trackpad accordingly.
* run ./contrib/ios.sh
* after it runs and the proper number of goats have been offered to the slaughter you will get an xz's tarball in the build/iphone/ directory

additional cmake flags can be passed to ./contrib/ios.sh as command line arguments like so:

    $ ./contrib/ios.sh -DYOLO_SWAG=ON
