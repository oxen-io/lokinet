This directory contains the magical incantations and random voodoo symbols needed to coax an Apple
build.  There's no reason builds have to be this stupid, except that Apple wants to funnel everyone
into the no-CI, no-help, undocumented, non-toy-apps-need-not-apply modern Apple culture.

This is disgusting.

But it gets worse.

The following two files, in particular, are the very worst manifestations of this already toxic
Apple cancer: they are required for proper permissions to run on macOS, are undocumented, and can
only be regenerated through the entirely closed source Apple Developer backend, for which you have
to pay money first to get a team account (a personal account will not work), and they lock the
resulting binaries to only run on individually selected Apple computers selected at the time the
profile is provisioned (with no ability to allow it to run anywhere).

    lokinet.provisionprofile
    lokinet-extension.provisionprofile

This is actively hostile to open source development, but that is nothing new for Apple.

In order to make things work, you'll have to replace these provisioning profiles with your own
(after paying Apple for the privilege of developing on their platform, of course) and change all the
team/application/bundle IDs to reference your own team, matching the provisioning profiles.  The
provisioning profiles must be a "macOS Development" provisioning profile, and must include the
signing keys and the authorized devices on which you want to run it.  (The profiles bundled in this
repository contains the lokinet team's "Apple Development" keys associated with the Oxen project,
and mac dev boxes.  This is *useless* for anyone else).

Also take note that you *must not* put a development build `lokinet.app` inside /Applications
because if you do, it won't work because *on top* of the ridiculous signing and entitlement bullshit
that Apple makes you jump through, the rules *also* differ for binaries placed in /Applications
versus binaries placed elsewhere, but like everything else here, it is entirely undocumented.

If you are reading this to try to build Lokinet for yourself for an Apple operating system and
simultaneously care about open source, privacy, or freedom then you, my friend, are a walking
contradiction: you are trying to get Lokinet to work on a platform that actively despises open
source, privacy, and freedom.  Even Windows is a better choice in all of these categories than
Apple.
