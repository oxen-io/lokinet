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
*------------------------------------------------------------------------------
* libhttp, a really small HTTP 0-3 client library with TLS
* public API header
* do not include this file in any of the libhttp sources itself
* 
* HTTPS only; why the hell would you serve semi-sensitive data over an
* unencrypted channel? In fact, the polarssl integration is intended to
* bypass limitations in the native TLS stack (no TLS 1.1+ on some older
* platforms, lack of high-encryption ciphersuites other than ARC4 or
* Triple-DES, etc)
* -rick
*/

#ifndef LIBHTTP_H
#define LIBHTTP_H

/* http client object */
typedef struct
{
	char *ua; /* platform-specific user-agent string */
	char *request_uri; /* last uri requested, the response corresponds to this link */
	struct responseBody
	{
		/* the raw_data and headers point to the same place */
		/* the content is an offset into the raw_data */
		union
		{
			char* raw_data;
			char* headers;
		};
		char* content;
	};
	/* anonymous field, do not poke */
	void *reserved;
} http_state;

/* libhttp public API */
bool init_client(http_state*);
int download_https_resource(http_state*, char*);
void terminate_client(http_state*);
#endif