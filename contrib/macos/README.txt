This directory contains the magical incantations and random voodoo symbols needed to coax an Apple
build.  There's no reason builds have to be this stupid, except that Apple wants to funnel everyone
into the no-CI, no-help, undocumented, non-toy-apps-need-not-apply modern Apple culture.

This is disgusting.




These two files, in particular, are the very worst manifestations of this Apple cancer: they are
required for proper permissions to run on macOS, are undocumented, and can only be regenerated
through the entirely closed source Apple Developer backend:

    lokinet.provisionprofile
    lokinet-extension.provisionprofile

This is actively hostile to open source development, but that is nothing new for Apple.

If you are reading this to try to build Lokinet for yourself for an Apple operating system and
simultaneously care about open source, privacy, or freedom then you, my friend, are a walking
contradiction: you are trying to get Lokinet to work on a platform that actively despises open
source, privacy, and freedom.  Even Windows is a better choice in all of these categories than
Apple.
