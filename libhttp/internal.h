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
* this file contains internal definitions of libhttp data structures
* internal.h and libhttp.h are mutually exclusive as they define the same
* data structures, library clients must use libhttp.h
*
*/

#ifndef INTERNAL_H
#define INTERNAL_H

/* PolarSSL */
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>
#include <mbedtls/base64.h>

/* function declarations */
void free_parsed_url();
parse_url();
void *memncat();

typedef struct url_parser_url 
{
	char *protocol;
	char *host;
	int port;
	char *path;
	char *query_string;
	int host_exists;
	char *host_ip;
} url_t;

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

	/* not a public field */
	struct
	{
		mbedtls_net_context server_fd;
		mbedtls_entropy_context entropy;
		mbedtls_ctr_drbg_context ctr_drbg;
		mbedtls_ssl_context ssl;
		mbedtls_ssl_config conf;
		mbedtls_x509_crt cacert;
		bool TLSInit;
		char seed[64];
	} tls_state;
} http_state;

#endif