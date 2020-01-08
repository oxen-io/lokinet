#!/bin/sh
VERSION=$1
EMAIL=$2
PW=$3

echo "cleaning old version"
rm -fr osx-pkg
echo "making structure"
mkdir -p osx-pkg/usr/local/bin
mkdir osx-pkg/usr/local/lib
mkdir osx-pkg/Applications

echo "Copying latest build"
rm osx-pkg/usr/local/bin/lokinet
cp ../build/daemon/lokinet osx-pkg/usr/local/bin
#cp ../lokinet osx-pkg/usr/local/bin
rm osx-pkg/usr/local/bin/lokinetctl
cp ../build/daemon/lokinetctl osx-pkg/usr/local/bin
echo "Copying /usr/local/lib/libuv.dylib into package"
rm osx-pkg/usr/local/lib/libuv.dylib
cp /usr/local/lib/libuv.dylib osx-pkg/usr/local/lib
# just incase they want to switch networks later
rm osx-pkg/usr/local/bin/lokinet-bootstrap
cp ../lokinet-bootstrap osx-pkg/usr/local/bin
#echo "Copying lokinet-control-panel.app"
rm -fr osx-pkg/Applications/Lokinet.app
# requires -R to keep all the symbolic links inside the App
# https://blog.inventic.eu/2015/03/os-x-codesign-failed-bundle-format-is-ambiguous-could-be-app-or-framework/
cp -R Lokinet.app osx-pkg/Applications
cp lokinet-brand_icon-only.icns osx-pkg/Applications/Lokinet.app/Contents/Resources/lokinet.icns
echo "Fixing CFBundleIdentifier and CFBundleIconFile"
# MacOS has to be different, ugh...
# https://stackoverflow.com/questions/32004950/how-to-replace-string-in-a-file-in-place-using-sed
sed -i '' s/com.yourcompany.lokicp/network.loki.lokinet/ osx-pkg/Applications/Lokinet.app/Contents/Info.plist
sed -i '' s/<string></string>/<string>lokinet.icns</string> osx-pkg/Applications/Lokinet.app/Contents/Info.plist

echo "Generating distribution version"
sed "s/\$VERSION/$VERSION/" distribution_template.xml > distribution.xml

echo "Signing package $VERSION"
codesign --options=runtime -s "Rangeproof PTY LTD" -v osx-pkg/usr/local/bin/lokinet
codesign --options=runtime -s "Rangeproof PTY LTD" -v osx-pkg/usr/local/bin/lokinetctl
codesign --options=runtime -s "Rangeproof PTY LTD" -v osx-pkg/usr/local/bin/lokinet-bootstrap
codesign --options=runtime -s "Rangeproof PTY LTD" -v osx-pkg/usr/local/lib/libuv.dylib
codesign --deep --options=runtime -s "Rangeproof PTY LTD" -v osx-pkg/Applications/Lokinet.app

echo "Building package $VERSION"
mkdir -p pkg1
rm pkg1/lokinet.pkg
pkgbuild --root osx-pkg --scripts scripts --identifier network.loki.lokinet --version $VERSION pkg1/lokinet.pkg
rm lokinet-v$VERSION.pkg
productbuild --distribution distribution.xml --resources resources --package-path pkg1 --version $VERSION lokinet_macos64_installer_v$VERSION.pkg
productsign --sign F87AE1FB2AEDAE8A15F9510B6DD1AB2B04E384A1 lokinet_macos64_installer_v$VERSION.pkg lokinet_macos64_installer_v$VERSION-signed.pkg
#pkgutil --check-signature lokinet_macos64_installer_v$VERSION-signed.pkg
spctl -a -v --type install lokinet_macos64_installer_v$VERSION-signed.pkg
xcrun altool --notarize-app --primary-bundle-id "network.loki.loki-network.pkg" --username $EMAIL --password $PW --asc-provider "SUQ8J2PCT7" --file lokinet_macos64_installer_v$VERSION-signed.pkg
echo "xcrun altool --notarization-info X --username $EMAIL --password $PW"
echo "when it's ready, run:"
echo "xcrun stapler staple lokinet_macos64_installer_v$VERSION-signed.pkg"
