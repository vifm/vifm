/*
 * Copyright (c) 2017, Alexandru Geana (alegen)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names "Alexandru Geana" or "alegen" nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _STRNREP_H_
#define _STRNREP_H_

/**
 * struct strmap_t - the basic map structure
 * @len:        total number of entries which can fit into the map
 * @n_entries:  current number of entries in the map
 * @entries:    the actual entries, stored as entries[i][0] for the key
 *              and entries[i][1] for the value of entry i; both key and
 *              value are null terminated strings
 *
 * This structure defines the strmap_t type used by the strmap_* functions
 * defined in file strnrep.c. Objects of this type can be allocated with
 * strmap_malloc() and freed with strmap_free().
 */
typedef struct {
    size_t len;
    size_t n_entries;
    char ***entries;
} strmap_t;

strmap_t * strmap_malloc(size_t mlen);
void strmap_free(strmap_t *map);
int strmap_add(strmap_t *map, char *key, char *val);
int strmap_set(strmap_t *map, char *key, char *val);
char * strnrep(char *out, char *fmt, size_t olen, strmap_t *map);

#endif // _STRNREP_H_
