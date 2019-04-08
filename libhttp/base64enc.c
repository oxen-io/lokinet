/*
* Copyright (c)2018-2019 Rick V. All rights reserved.
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

/* this is a tiny build-time utility that base64 encodes up to 512K
 * of text/binary data from stdin. (On UNIX, we'd use GNU's [g]base64(1)
 * to encode the stream. Can't guarantee that a windows user will have cygwin
 * installed, so we bootstrap these at build-time instead.)
 *
 * here, it is used to encode the compressed zlib-stream of the 
 * Netscape root certificate trust store on behalf of the lokinet
 * for NT bootstrap stubs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sysconf.h"
#ifdef HAVE_SETMODE
# define SET_BINARY_MODE(handle) setmode(handle, O_BINARY)
#else
# define SET_BINARY_MODE(handle) ((void)0)
#endif
#include <mbedtls/base64.h>
#include <mbedtls/error.h>

main(argc, argv)
char** argv;
{
	int size,r, inl;
	unsigned char in[524288];
	unsigned char out[1048576];
	unsigned char err[1024];
	memset(&in, 0, 524288);
	memset(&out, 0, 1048576);
	SET_BINARY_MODE(0);
	/* Read up to 512K of data from stdin */
	inl = fread(in, 1, 524288, stdin);
	r = mbedtls_base64_encode(out, 1048576, &size, in, inl);
	if (r)
	{
		mbedtls_strerror(r, err, 1024);
		printf("error: %s\n", err);
		return r;
	}
	fprintf(stdout, "%s", out);
	return 0;
}