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
* 
* HTTPS only; why the hell would you serve semi-sensitive data over an
* unencrypted channel? In fact, the polarssl integration is intended to
* bypass limitations in the native TLS stack (no TLS 1.1+ on some older
* platforms, lack of high-encryption ciphersuites other than ARC4 or
* Triple-DES, etc)
* -rick
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#include "miniz.h"
#include "internal.h"

/* only decompress rootcerts once */
unsigned char* ca_certs = NULL;
/* netscape ca bundle */
const unsigned char *ca_cert_store_encoded;
/* imageboard ref just because */
static char userAgent[] = "NetRunner_Micro/0.1 PolarSSL/2.16.0;U;";

static void destroy_persistent_data()
{
	free(ca_certs);
	ca_certs = NULL;
}

static bool generateSeed(client)
http_state *client;
{
#ifdef _WIN32
	HCRYPTPROV hprovider;

	/* On Windows NT 4.0 or later, use CryptoAPI to grab 64 bytes of random data */
	hprovider = 0;
	CryptAcquireContext(&hprovider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
	CryptGenRandom(hprovider, 64, (BYTE*)&client->tls_state.seed);
	CryptReleaseContext(hprovider, 0);
#endif
	client->tls_state.seed[63] = '\0';  /* null-terminate for safety */
	return true;
}

static bool initTLS(client)
http_state *client;
{
	int inf_status,r;
	size_t out;
	unsigned long inf_len;
	unsigned char* tmp;
	char str[512];

	mbedtls_net_init(&client->tls_state.server_fd);
	mbedtls_ssl_init(&client->tls_state.ssl);
	mbedtls_ssl_config_init(&client->tls_state.conf);
	mbedtls_x509_crt_init(&client->tls_state.cacert);
	mbedtls_entropy_init(&client->tls_state.entropy);
	mbedtls_ctr_drbg_init(&client->tls_state.ctr_drbg);

	/* only decompress once */
	if (!ca_certs)
	{
		tmp = malloc(524288);

		r = strlen(ca_cert_store_encoded) - 1;
		r = mbedtls_base64_decode(tmp, 524288, &out, ca_cert_store_encoded, r);
		if (r)
		{
			mbedtls_strerror(r, (char*)tmp, 524288);
			printf("decoding failed: %s\n", tmp);
			free(tmp);
			return false;
		}
		/* inflate ca certs, they are still compressed */
		ca_certs = malloc(524288);
		inf_len = 524288;

		inf_status = uncompress(ca_certs, &inf_len, tmp, out);
		if (inf_status != Z_OK)
		{
			printf("decompression failed: %s\n", mz_error(inf_status));
			free(tmp);
			return false;
		}
		free(tmp);
	}

	if (!generateSeed())
		return false;

	if (mbedtls_ctr_drbg_seed(&client->tls_state.ctr_drbg, mbedtls_entropy_func, &client->tls_state.entropy, (const unsigned char*)client->tls_state.seed, 64)) {
		return false;
	}

	r = mbedtls_x509_crt_parse(&client->tls_state.cacert, ca_certs, inf_len+1);
	if (r < 0) {
		mbedtls_strerror(r, str, 512);
		printf("parse ca cert store failed\n  !  mbedtls_x509_crt_parse returned: %s\n\n", str);
		return false;
	}
	client->tls_state.TLSInit = true;
	return true;
}

/* if false, library may be in an inconsistent state,
* call terminate_client()
*/
bool init_client(client)
http_state* client;
{
	if (!ca_certs)
		atexit(destroy_persistent_data);
	if (!client)
		client = calloc(1, sizeof(http_state));
	initTLS(client);

	return client->tls_state.TLSInit;
}

static void ua_string(client)
http_state *client;
{
	/* fill in user-agent string */
#ifdef _WIN32
	DWORD version, major, minor, build;
	version = GetVersion();
	major = (DWORD)(LOBYTE(LOWORD(version)));
	minor = (DWORD)(HIBYTE(LOWORD(version)));
	if (version < 0x80000000)
		build = (DWORD)(HIWORD(version));
	client->ua = malloc(512);
	snprintf(client->ua, 512, "%sWindows NT %d.%d", userAgent, major, minor);
#endif
}

download_https_resource(client, uri)
http_state *client;
char *uri;
{
	int r, len;
	char buf[1024], port[8];
	char *rq, *resp;
	unsigned flags;
	url_t *parsed_uri;

	rq = malloc(4096);
	/* this string gets readjusted each time we make a request */
	client->request_uri = realloc(NULL, strlen(uri)+1);
	parsed_uri = malloc(sizeof(url_t));
	memset(parsed_uri, 0, sizeof(url_t));
	r = parse_url(uri, false, parsed_uri);
	if (r)
	{
		printf("Invalid URI pathspec\n");
		return -1;
	}

	if (!client->tls_state.TLSInit)
	{
		printf("Failed to initialise polarssl\n");
		return -1;
	}

	/* get host name, set port if blank */
	if (!strcmp("https", parsed_uri->protocol) && !parsed_uri->port)
		parsed_uri->port = 443;

	printf("connecting to %s on port %d...",parsed_uri->host, parsed_uri->port);
	sprintf(port, "%d", parsed_uri->port);
	r = mbedtls_net_connect(&client->tls_state.server_fd, parsed_uri->host, port, MBEDTLS_NET_PROTO_TCP);
	if (r)
	{
		printf("error - failed to connect to server: %d\n", r);
		goto exit;
	}

	r = mbedtls_ssl_config_defaults(&client->tls_state.conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	if (r)
	{
		printf("error - failed to set TLS options: %d\n", r);
		goto exit;
	}
	mbedtls_ssl_conf_authmode(&client->tls_state.conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&client->tls_state.conf, &client->tls_state.cacert, NULL);
	mbedtls_ssl_conf_rng(&client->tls_state.conf, mbedtls_ctr_drbg_random, &client->tls_state.ctr_drbg);
	r = mbedtls_ssl_setup(&client->tls_state.ssl, &client->tls_state.conf);
	if (r)
	{
		printf("error - failed to setup TLS session: %d\n", r);
		goto exit;
	}
	r = mbedtls_ssl_set_hostname(&client->tls_state.ssl, parsed_uri->host);
	if (r)
	{
		printf("error - failed to perform SNI: %d\n", r);
		goto exit;
	}
	mbedtls_ssl_set_bio(&client->tls_state.ssl, &client->tls_state.server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
	while ((r = mbedtls_ssl_handshake(&client->tls_state.ssl)) != 0)
	{
		if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			printf(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n", -r);
			goto exit;
		}
	}
	if ((flags = mbedtls_ssl_get_verify_result(&client->tls_state.ssl)) != 0)
	{
		char vrfy_buf[512];
		printf(" failed\n");
		mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
		printf("%s\n", vrfy_buf);
		goto exit;
	}
	printf("\nDownloading %s...", &parsed_uri->path[1]);
	snprintf(rq, 512, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n", parsed_uri->path, parsed_uri->host, client->ua);
	while ((r = mbedtls_ssl_write(&client->tls_state.ssl, (unsigned char*)rq, strlen(rq))) <= 0)
	{
		if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			printf("failed! error %d\n\n", r);
			goto exit;
		}
	}
	memset(rq, 0, 4096);
	len = 0;
	do {
		r = mbedtls_ssl_read(&client->tls_state.ssl, (unsigned char*)buf, 1024);
		if (r <= 0)
			break;
		else 
		{
			rq = memncat(rq, len, buf, r, sizeof(char));
			len += r;
		}
	} while (r);
	printf("%d bytes downloaded to core.\n", len);
	mbedtls_ssl_close_notify(&client->tls_state.ssl);
	if (!strstr(rq, "200 OK"))
	{
		printf("An error occurred.\n");
		printf("Server response:\n%s", rq);
		goto exit;
	}

	/* Response body is in buf after processing */
	resp = strstr(rq, "Content-Length");
	r = strcspn(resp, "0123456789");
	memcpy(buf, &resp[r], 4);
	buf[3] = '\0';
	r = atoi(buf);
	resp = strstr(rq, "\r\n\r\n");
	memcpy(buf, &resp[4], r);

	r = 0;

exit:
	free(rq);
	free_parsed_url(parsed_uri);
	return r;
}

void terminate_client(client)
http_state *client;
{
	mbedtls_ssl_close_notify(&client->tls_state.ssl);
	mbedtls_net_free(&client->tls_state.server_fd);
	mbedtls_x509_crt_free(&client->tls_state.cacert);
	mbedtls_ssl_free(&client->tls_state.ssl);
	mbedtls_ssl_config_free(&client->tls_state.conf);
	mbedtls_ctr_drbg_free(&client->tls_state.ctr_drbg);
	mbedtls_entropy_free(&client->tls_state.entropy);
	free(client->ua);
}