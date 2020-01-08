@echo off
copy lokinet.ini lokinet.old.ini
del lokinet.ini
%PROGRAMFILES%\Loki Project\Lokinet\lokinet -g