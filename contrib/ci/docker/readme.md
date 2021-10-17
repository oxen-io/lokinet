## drone-ci docker jizz

To rebuild all ci images and push them to the oxen registry server do:

    $ docker login registry.oxen.rocks
    $ ./rebuild-docker-images.sh

If you aren't part of the Oxen team, you'll likely need to set up your own registry and change
registry.oxen.rocks to your own domain name in order to do anything useful with this.

The docker images will be `registry.oxen.rocks/lokinet-ci-*`for each \*.dockerfile in this
directory, with the leading numeric `NN-` removed, if present (so that you can ensure proper
ordering using two-digit numeric prefixes).
