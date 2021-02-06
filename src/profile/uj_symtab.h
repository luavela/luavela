/*
 * Lua symbol table dumper.
 *
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_SYMTAB_H
#define _UJ_SYMTAB_H

#include <stdint.h>

struct global_State;
struct ujp_buffer;

/*
 * symtab format:
 *
 * symtab         := prologue sym*
 * prologue       := 'u' 'j' 's' version reserved vm-id
 * version        := <BYTE>
 * reserved       := <BYTE> <BYTE> <BYTE>
 * vm-id          := <ULEB128>
 * sym            := sym-lua | sym-final
 * sym-lua        := sym-header sym-addr sym-chunk sym-line
 * sym-header     := <BYTE>
 * sym-addr       := <ULEB128>
 * sym-chunk      := string
 * sym-line       := <ULEB128>
 * sym-final      := sym-header
 * string         := string-len string-payload
 * string-len     := <ULEB128>
 * string-payload := <BYTE> {string-len}
 *
 * <BYTE>   :  A single byte (no surprises here)
 * <ULEB128>:  Unsigned integer represented in ULEB128 encoding
 *
 * (Order of bits below is hi -> lo)
 *
 * version: [VVVVVVVV]
 *  * VVVVVVVV: Byte interpreted as a plain numeric version number
 *
 * sym-header: [FUUUUUTT]
 *  * TT    : 2 bits for representing symbol type
 *  * UUUUU : 5 unused bits
 *  * F     : 1 bit marking the end of the symtab (final symbol)
 */

#define SYMTAB_LFUNC ((uint8_t)0)
#define SYMTAB_CFUNC ((uint8_t)1)
#define SYMTAB_FFUNC ((uint8_t)2)
#define SYMTAB_TRACE ((uint8_t)3)
#define SYMTAB_FINAL ((uint8_t)0x80)

/* Writes the symbol table of the VM g to out. */
void uj_symtab_write(struct ujp_buffer *out, const struct global_State *g);

#endif /* !_UJ_SYMTAB_H */
