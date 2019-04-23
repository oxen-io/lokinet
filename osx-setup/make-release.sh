#!/bin/sh
VERSION=$1

echo "Copying latest build"
mkdir -p osx-pkg/usr/local/bin
cp ../lokinet osx-pkg/usr/local/bin
# just incase they want to switch networks later
cp ../lokinet-bootstrap osx-pkg/usr/local/bin

echo "Building package $VERSION"
mkdir -p pkg1
rm pkg1/lokinet.pkg
pkgbuild --root osx-pkg --scripts scripts --identifier network.loki.lokinet --version $VERSION pkg1/lokinet.pkg
rm lokinet-v0.4.pkg
productbuild --distribution distribution.xml --resources resources --package-path pkg1 --version $VERSION lokinet-v$VERSION.pkg

