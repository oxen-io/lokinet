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
* uri parsing functions
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "internal.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

void free_parsed_url(url_parsed)
url_t *url_parsed;
{
	if (url_parsed->protocol)
		free(url_parsed->protocol);
	if (url_parsed->host)
		free(url_parsed->host);
	if (url_parsed->path)
		free(url_parsed->path);
	if (url_parsed->query_string)
		free(url_parsed->query_string);

	free(url_parsed);
}

parse_url(url, verify_host, parsed_url)
char *url;
bool verify_host;
url_t *parsed_url;
{
	char *local_url, *token, *token_host, *host_port, *host_ip, *token_ptr;
	char *host_token_ptr, *path = NULL;

	/* Copy our string */
	local_url = strdup(url);

	token = strtok_r(local_url, ":", &token_ptr);
	parsed_url->protocol = strdup(token);

	/* Host:Port */
	token = strtok_r(NULL, "/", &token_ptr);
	if (token)
		host_port = strdup(token);
	else
		host_port = (char *) calloc(1, sizeof(char));

	token_host = strtok_r(host_port, ":", &host_token_ptr);
	parsed_url->host_ip = NULL;
	if (token_host) {
		parsed_url->host = strdup(token_host);

		if (verify_host) {
			struct hostent *host;
			host = gethostbyname(parsed_url->host);
			if (host != NULL) {
				parsed_url->host_ip = inet_ntoa(* (struct in_addr *) host->h_addr);
				parsed_url->host_exists = 1;
			} else {
				parsed_url->host_exists = 0;
			}
		} else {
			parsed_url->host_exists = -1;
		}
	} else {
		parsed_url->host_exists = -1;
		parsed_url->host = NULL;
	}

	/* Port */
	token_host = strtok_r(NULL, ":", &host_token_ptr);
	if (token_host)
		parsed_url->port = atoi(token_host);
	else
		parsed_url->port = 0;

	token_host = strtok_r(NULL, ":", &host_token_ptr);
	assert(token_host == NULL);

	token = strtok_r(NULL, "?", &token_ptr);
	parsed_url->path = NULL;
	if (token) {
		path = (char *) realloc(path, sizeof(char) * (strlen(token) + 2));
		memset(path, 0, sizeof(char) * (strlen(token)+2));
		strcpy(path, "/");
		strcat(path, token);

		parsed_url->path = strdup(path);

		free(path);
	} else {
		parsed_url->path = (char *) malloc(sizeof(char) * 2);
		strcpy(parsed_url->path, "/");
	}

	token = strtok_r(NULL, "?", &token_ptr);
	if (token) {
		parsed_url->query_string = (char *) malloc(sizeof(char) * (strlen(token) + 1));
		strncpy(parsed_url->query_string, token, strlen(token));
	} else {
		parsed_url->query_string = NULL;
	}

	token = strtok_r(NULL, "?", &token_ptr);
	assert(token == NULL);

	free(local_url);
	free(host_port);
	return 0;
}
