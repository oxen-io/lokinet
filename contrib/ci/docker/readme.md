## drone-ci docker jizz

To rebuild all ci images and push them to the oxen registry server do:

    $ docker login registry.oxen.rocks
    $ ./rebuild-docker-images.py

If you aren't part of the Oxen team, you'll likely need to set up your own registry and change
registry.oxen.rocks to your own domain name in order to do anything useful with this.
