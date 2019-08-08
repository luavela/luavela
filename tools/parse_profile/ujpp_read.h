/*
 * Interfaces for low-level reading.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_READ_H
#define _UJPP_READ_H

#include <inttypes.h>
#include <stddef.h>

struct parser_state;
struct reader;

/* Read uint64_t */
uint64_t ujpp_read_u64(struct reader *r);
/* Read uint8_t */
uint8_t ujpp_read_u8(struct reader *r);
/* Read string defined by len:str */
char *ujpp_read_str(struct reader *r);
/* Inititalize internal reader */
void ujpp_read_init(struct reader *r, const char *fname);
/* Free memory and close file */
void ujpp_read_terminate(struct reader *r);

#endif /* !_UJPP_READ_H */
