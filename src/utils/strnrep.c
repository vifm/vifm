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

#include <string.h>
#include <stdlib.h>

#include "strnrep.h"

/**
 * strmap_malloc() - allocate memory for a strmap_t object
 * @mlen:       the maximum number of entries which can fit into the strmap
 *
 * The strmap_malloc() function allocates memory for one strmap_t object.
 * Internally it uses malloc. The return value is a pointer to the newly
 * allocated object. This pointer can be used further with other strmap_*
 * functions.
 *
 * Return:      the return value is a pointer to a strmap_t* object
 */
strmap_t * strmap_malloc(size_t mlen) {
    size_t i;
    strmap_t *map;

    map = (strmap_t*)malloc( sizeof(strmap_t) );
    if (map == NULL)
        goto fail_rollback;
    memset(map, 0, sizeof(strmap_t));

    map->len = mlen;
    map->n_entries = 0;
    map->entries = (char***)malloc( sizeof(char**) * mlen );
    if (map->entries == NULL)
        goto fail_rollback;

    for (i = 0; i < mlen; i++) {
        map->entries[i] = (char**)malloc( sizeof(char*) * 2 );
        if (map->entries[i] == NULL)
            goto fail_rollback;
    }

    return map;

fail_rollback:
    strmap_free(map);
    return NULL;
}

/**
 * strmap_free() - free memory allocated for a strmap_t object
 * @map:        the strmap_t object to be freed
 *
 * The strmap_free() function is used to free memory allocated for a strmap_t
 * object. The function uses free() internally.
 */
void strmap_free(strmap_t *map) {
    size_t i;

    if (map != NULL) {
        if (map->entries != NULL) {
            for (i = 0; i < map->len; i++) {
                if (map->entries[i] != NULL)
                    free( map->entries[i] );
            }
            free(map->entries);
        }
        free(map);
    }
}

/**
 * strmap_add() - add a new entry to a strmap_t object
 * @map:        the strmap_t object on which to perform the action
 * @key:        the key of the new map entry
 * @val:        the value of the new map entry
 *
 * The strmap_add() function tries to insert one new entry into the map. The
 * entry is specified as a combination of the key and val pair. If the action
 * succeeds then the function retuns the value 0. If the action fails, the
 * function returns a non-zero value.
 *
 * Return:      0 on success and non-zero on failure
 */
int strmap_add(strmap_t *map, char *key, char *val) {
    if (map == NULL || key == NULL || val == NULL)
        return 1;

    if (map->len > map->n_entries) {
        map->entries[ map->n_entries ][0] = strdup(key);
        map->entries[ map->n_entries ][1] = strdup(val);

        if (map->entries[ map->n_entries ][0] == NULL ||
            map->entries[ map->n_entries ][1] == NULL)
            return 1;

        map->n_entries++;
    } else
        return 1;
    return 0;
}

/**
 * strmap_set() - set a new value for the entry with the specified key
 * @map:        the strmap_t onject on which to perform the action
 * @key:        the key of the entry to be changed
 * @val:        the new value of the entry
 *
 * The strmap_set() function is used to modify the value of an entry with the
 * specified key. If an entry with the specified key does not exist, the
 * strmap_add() function is called internally. The mstrap_set() function returns
 * 0 on success and a non-zero value on failure.
 *
 * Return:      0 on success and non-zero on failure
 */
int strmap_set(strmap_t *map, char *key, char *val) {
    size_t i;

    if (map == NULL || key == NULL || val == NULL)
        return 1;

    for (i = 0; i < map->n_entries; i++) {
        if (map->entries[i][0] == NULL ||
            map->entries[i][1] == NULL)
            return 1;

        if ( strcmp(key, map->entries[i][0]) == 0 ) {
            free(map->entries[i][1]);
            map->entries[i][1] = strdup(val);
            if (map->entries[i][1] == NULL)
                return 1;

            // done, exit the function
            return 0;
        }
    }

    // if we reach this point, it means that the key
    // was not found in the map so we try to add it
    return strmap_add(map, key, val);
}

/**
 * strnrep() - replace keys with values in string fmt
 * @out:        the buffer in which the final result is placed
 * @fmt:        the format string containing keys
 * @olen:       length in bytes of the out buffer
 * @map:        the strmap_t object containing keys and values
 *
 * The strnrep() function replaces occurences of keys with the value of the same
 * entry. Pairs of keys and values are specified by the map argument. The map
 * argument can be created and manipulated using the strmap_* functions. The
 * olen argument specifies the maximum size of the final string.
 *
 * Return: the value of the out argument on success and NULL on failure
 */
char * strnrep(char *out, char *fmt, size_t olen, strmap_t *map) {
    size_t keylen, vallen, d, i;
    char *m, *aux;

    if (fmt == NULL || out == NULL || map == NULL)
        return NULL;

    // allocate a scrap buffer
    aux = (char*)malloc(olen);
    if (aux == NULL)
        return NULL;
    memset(aux, 0, olen);

    strncpy(out, fmt, olen);

    // ensure out is a valid null terminated string
    out[olen - 1] = '\0';

    for (i = 0; i < map->n_entries; i++) {
        if (map->entries[i] == NULL ||
            map->entries[i][0] == NULL ||
            map->entries[i][1] == NULL)
            continue;

        m = strstr(out, map->entries[i][0]);
        if (m != NULL) {
            keylen = strlen(map->entries[i][0]);
            vallen = strlen(map->entries[i][1]);

            d = m - out;

            if (olen >= d) {
                // copy the characters until the start of the format sequence
                strncpy(aux, out, d);

                // copy the mapping
                strncpy(aux + d, map->entries[i][1], olen - d);

                if (olen >= d + vallen) {
                    // copy the remaining part of the initial string
                    strncpy(aux + d + vallen, out + d + keylen, olen - d - vallen);
                }

                // ensure aux is a valid null terminated string
                aux[olen - 1] = '\0';
            } else {
                continue;
            }

            strncpy(out, aux, olen);
            // ensure out is a valid null terminated string
            out[olen - 1] = '\0';
        }
    }

    return out;
}
