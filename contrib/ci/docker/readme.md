## drone-ci docker jizz

To rebuild all ci images and push them to a registry server do:

    $ docker login your.registry.here
    $ ./rebuild-docker-images.sh your.registry.here *.dockerfile

The docker images will be `your.registry.here/lokinet-ci-*`for each *.dockerfile in this directory
