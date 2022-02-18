/*
 * Utilities for working with randomness.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_UTILS_RANDOM_H
#define _UJIT_UTILS_RANDOM_H

#include <stdlib.h>

/* Choose and return an initial random seed based on the current time.
** Based on code by Lawrence Kirby <fred@genesis.demon.co.uk> and
** http://benpfaff.org/writings/clc/random-seed.html
** Example usage: srandom(random_time_seed());
*/
unsigned int random_time_seed();

/* Generate a random file extension consisting of the initial '.' character
** and (n - 1) random lower-case hexadecimal digits. Generated result is written
** to buffer, buffer overflow is not checked.
*/
void random_hex_file_extension(char *buffer, size_t n);

#endif /* !_UJIT_UTILS_RANDOM_H */
