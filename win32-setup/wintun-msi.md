## Example Standalone MSI

The best way to include Wintun in your software is by including the MSMs in your final MSI,
as described by [the main README](../README.md). However, if you're stuck with an installation
system such as NSIS, which can not bundle MSM files, then you must build your own MSI, which
NSIS can then invoke. ***Do not use an MSI from elsewhere. You must build it yourself and
distribute only the MSI that you yourself build.*** Otherwise different projects will wind up
uninstalling each other by accident and disturbing the MSM reference counting. The steps in
this file should only be taken if you're not able to include an MSM into a MSI, something that
is easily possible using WiX or most commercial installation solutions.

This `msi-example` folder contains a WiX skeleton and a build script that handles all
dependencies. use it as follows below.

#### Steps:

1. Generate a UUID using uuidgen.exe and replace `{{{FIXED 64BIT UUID}}}` in exampletun.wxs
with that UUID. For the life time of your entire product, even across versions, do not change
that UUID.

2. Generate another UUID using uuidgen.exe and replace `{{{FIXED 32BIT UUID}}}` in
exampletun.wxs with that UUID. For the life time of your entire product, even across versions,
do not change that UUID.

3. Go to [Wintun.net](https://www.wintun.net/) and look at what the latest version is (`0.6`,
for example). Replace `{{{VERSION}}}` in build.bat with that version.

4. Download the amd64 MSM from [Wintun.net](https://www.wintun.net/) and compute its SHA2-256
sum in all lowercase hex digits using `CertUtil -hashfile "path/to/file" SHA256`, and replace
`{{{64BIT HASH}}}` in build.bat with that value.

5. Download the x86 MSM from [Wintun.net](https://www.wintun.net/) and compute its SHA2-256
sum in all lowercase hex digits using `CertUtil -hashfile "path/to/file" SHA256`, and replace
`{{{32BIT HASH}}}` in build.bat with that value.

6. Run build.bat.

7. Distribute dist\exampletun-*.msi for your own software only.

