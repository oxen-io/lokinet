# Makefile for windows install pkg and helper library

regdbhelper.dll:
	i686-w64-mingw32-gcc regdb_helper.c -o $@ -shared -Os -s
clean:
	rm *.dll *.exe
