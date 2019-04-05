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
* internal utility functions for libhttp
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "internal.h"

void *memncat(a, an, b, bn, s)
const void *a, *b;
size_t an, bn, s;
{
	char *p = malloc(s * (an + bn));
	memset(p, '\0', s * (an + bn));
	memcpy(p, a, an*s);
	memcpy(p + an*s, b, bn*s);
	return p;
}