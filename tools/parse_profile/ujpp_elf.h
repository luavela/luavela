/*
 * Interfaces for elf64 object file parsing.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJPP_ELF_H
#define _UJPP_ELF_H

struct vector;

/*
 * Parses elf64 file and saves extracted (and demangled for C++) symbols to
 * the vector.
 */
void ujpp_elf_parse_file(const char *path, struct vector *v);

/* Reads symbols from the buffer. */
void ujpp_elf_parse_mem(const char *buf, struct vector *v);

/*
 * Returns size of code section with one remark: in case if binary was build
 * as non-PIE, returned size will be .text section link address + it's size.
 * We assume that binary occupies memory from 0 to the end of .text section:
 * [0; code link base + code section size].
 */
size_t ujpp_elf_text_sz(const char *path);

#endif /* !_UJPP_ELF_H */
