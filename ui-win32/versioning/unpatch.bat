REM this requires some kind of cygwin toolset in $PATH
REM or just having sed(1) at a minimum
set /P RELEASE_NAME=<..\..\..\motto.txt
sed -i "s/%RELEASE_NAME%/RELEASE_CODENAME/g" ..\..\Properties\AssemblyInfo.cs