@echo off
rem SPDX-License-Identifier: GPL-2.0
rem Copyright (C) 2019 WireGuard LLC. All Rights Reserved.

setlocal
set PATHEXT=.exe
set BUILDDIR=%~dp0
cd /d %BUILDDIR% || exit /b 1

set WIX_CANDLE_FLAGS=-nologo
set WIX_LIGHT_FLAGS=-nologo -spdb -sice:ICE71 -sice:ICE61

if exist .deps\prepared goto :build
:installdeps
	rmdir /s /q .deps 2> NUL
	mkdir .deps || goto :error
	cd .deps || goto :error
	call :download wintun-x86.msm https://www.wintun.net/builds/wintun-x86-{{{VERSION}}}.msm {{{32BIT HASH}}} || goto :error
	call :download wintun-amd64.msm https://www.wintun.net/builds/wintun-amd64-{{{VERSION}}}.msm {{{64BIT HASH}}} || goto :error
	call :download wix-binaries.zip http://wixtoolset.org/downloads/v3.14.0.2812/wix314-binaries.zip 923892298f37514622c58cbbd9c2cadf2822d9bb53df8ee83aaeb05280777611 || goto :error
	echo [+] Extracting wix-binaries.zip
	mkdir wix\bin || goto :error
	tar -xf wix-binaries.zip -C wix\bin || goto :error
	echo [+] Cleaning up wix-binaries.zip
	del wix-binaries.zip || goto :error
	copy /y NUL prepared > NUL || goto :error
	cd .. || goto :error

:build
	set WIX=%BUILDDIR%.deps\wix\
	call :msi x86 i686 x86 || goto :error
	call :msi amd64 x86_64 x64 || goto :error
	if exist ..\sign.bat call ..\sign.bat
	if "%SigningCertificate%"=="" goto :success
	if "%TimestampServer%"=="" goto :success
	echo [+] Signing
	signtool sign /sha1 "%SigningCertificate%" /fd sha256 /tr "%TimestampServer%" /td sha256 /d "ExampleTun Setup" "dist\exampletun-*.msi" || goto :error

:success
	echo [+] Success.
	exit /b 0

:download
	echo [+] Downloading %1
	curl -#fLo %1 %2 || exit /b 1
	echo [+] Verifying %1
	for /f %%a in ('CertUtil -hashfile %1 SHA256 ^| findstr /r "^[0-9a-f]*$"') do if not "%%a"=="%~3" exit /b 1
	goto :eof

:msi
	if not exist "%~1" mkdir "%~1"
	echo [+] Compiling %1
	"%WIX%bin\candle" %WIX_CANDLE_FLAGS% -dEXAMPLETUN_PLATFORM="%~1" -out "%~1\exampletun.wixobj" -arch %3 exampletun.wxs || exit /b %errorlevel%
	echo [+] Linking %1
	"%WIX%bin\light" %WIX_LIGHT_FLAGS% -out "dist\exampletun-%~1.msi" "%~1\exampletun.wixobj" || exit /b %errorlevel%
	goto :eof

:error
	echo [-] Failed with error #%errorlevel%.
	cmd /c exit %errorlevel%
