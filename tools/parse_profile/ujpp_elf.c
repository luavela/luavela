/*
 * Elf64 symbols reader.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <elf.h>
#include <string.h>
#include <assert.h>

#include "ujpp_demangle.h"
#include "ujpp_utils.h"

#define TRUNCATE_LEN 90
#define SECTION_NOT_FOUND (size_t)(-1)

/*
 * GCC can't find cxxabi.h by default and enable_language(CXX) in cmake will
 * break Ubuntu 18 build. So lets use forward declaration.
 */
char *__cxa_demangle(const char *__mangled_name, char *__output_buffer,
		     size_t *__length, int *__status);

static const Elf64_Shdr *
elf_read_section_field(const char *buf, const Elf64_Ehdr *ehdr, size_t index)
{
	return (Elf64_Shdr *)(buf + ehdr->e_shoff + index * ehdr->e_shentsize);
}

static const Elf64_Phdr *
elf_read_header_field(const char *buf, const Elf64_Ehdr *ehdr, size_t index)
{
	return (Elf64_Phdr *)(buf + ehdr->e_phoff + index * ehdr->e_phentsize);
}

/* Satisfied that the file is a x86_64 ELF shared / executable object. */
static void elf_check_file(const void *buf)
{
	Elf64_Ehdr *hdr = (Elf64_Ehdr *)buf;

	if (ELFMAG0 != hdr->e_ident[EI_MAG0] ||
	    ELFMAG1 != hdr->e_ident[EI_MAG1] ||
	    ELFMAG2 != hdr->e_ident[EI_MAG2] ||
	    ELFMAG3 != hdr->e_ident[EI_MAG3])
		ujpp_utils_die("Wrong magic number", NULL);

	if (ET_DYN != hdr->e_type && ET_EXEC != hdr->e_type)
		ujpp_utils_die("Isn't shared or executalbe object", NULL);

	if (ELFCLASS64 != hdr->e_ident[EI_CLASS])
		ujpp_utils_die("Not a 64-bit object", NULL);

	if (ELFDATA2LSB != hdr->e_ident[EI_DATA])
		ujpp_utils_die("Wrong endianness", NULL);
}

static const char *elf_get_section_name(const char *buf,
					const Elf64_Shdr *section)
{
	const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)buf;
	const Elf64_Shdr *shstr =
		elf_read_section_field(buf, ehdr, ehdr->e_shstrndx);

	return buf + shstr->sh_offset + section->sh_name;
}

static size_t elf_get_strsect_offset(const char *buf, const char *sect)
{
	const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)buf;

	/* Start from 1 because 0 is a SHN_UNDEF index. */
	for (size_t i = 1; i < ehdr->e_shnum && i < SHN_LORESERVE; i++) {
		const Elf64_Shdr *shdr = elf_read_section_field(buf, ehdr, i);

		if (strcmp(elf_get_section_name(buf, shdr), sect) != 0)
			continue;

		if (SHT_STRTAB == shdr->sh_type) {
			assert(*(buf + shdr->sh_offset) == 0);
			return (size_t)shdr->sh_offset;
		}
	}

	return SECTION_NOT_FOUND;
}

/*
 * There isn't single mangling standard for C++ and each compiler has it's own
 * implementation. _Z at the beginning means that this is a GCC / LLVM /
 * Intel C++ compiler symbol (they have compatible mangling scheme).
 */
static int elf_is_gcc_symbol(const char *sym)
{
	return '_' == sym[0] && 'Z' == sym[1];
}

static void elf_trunc_sym(char *sym)
{
	size_t len = strlen(sym);

	if (len > TRUNCATE_LEN) {
		/* Not very beautiful truncation. */
		sym[TRUNCATE_LEN] = '\0';
	}
}

static const char *elf_copy_symbol(const char *orig_sym, int need_copy)
{
	size_t len = strlen(orig_sym) + 1;
	char *sym = (char *)orig_sym;

	/* '\0' string is possible. */
	if (len == 1)
		return NULL;

	if (need_copy) {
		sym = ujpp_utils_allocz(len);
		memcpy(sym, orig_sym, len - 1);
		sym[len - 1] = '\0';
	}

	elf_trunc_sym(sym);

	return sym;
}

static const char *elf_sym_name(const char *buf, Elf64_Word name,
				size_t strtab_offset)
{
	const char *sym_offset = buf + strtab_offset + name;
	const char *sym_name = sym_offset;
	/* Symbol must be copied to the new allocated memory. */
	int need_copy = 1;

	if (elf_is_gcc_symbol(sym_offset)) {
		sym_name = __cxa_demangle(sym_offset, NULL, NULL, NULL);

		/* Demangling failed, use original symbol. */
		if (NULL == sym_name)
			sym_name = sym_offset;
		else
			need_copy = 0;
	}

	return elf_copy_symbol(sym_name, need_copy);
}

/* Extracting symbols from section. */
static void elf_parse_symtab(const char *buf, const Elf64_Shdr *shdr,
			     size_t str_idx, struct vector *v)
{
	Elf64_Sym *sym;
	const char *symtab = buf + shdr->sh_offset;

	/* Section must contain integer number of entries. */
	assert(shdr->sh_size % sizeof(*sym) == 0);

	for (size_t i = 0; i < (shdr->sh_size / sizeof(*sym)); i++) {
		struct sym_info *si;
		const char *sym_name;

		sym = (Elf64_Sym *)(symtab + i * sizeof(*sym));

		/* We need only functions with defined sizes. */
		if (STT_FUNC != ELF64_ST_TYPE(sym->st_info) ||
		    0 == sym->st_size)
			continue;

		sym_name = elf_sym_name(buf, sym->st_name, str_idx);

		if (!sym_name)
			continue;

		si = ujpp_utils_allocz(sizeof(*si));
		si->name = sym_name;
		si->size = sym->st_size;
		si->addr = sym->st_value;

		ujpp_vector_add(v, si);
	}
}

static void elf_iterate_sections(const char *buf, struct vector *syms,
				 Elf32_Word section_type, size_t str_index)
{
	const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)buf;

	if (SECTION_NOT_FOUND == str_index)
		return;

	/* Start from 1 because 0 is a SHN_UNDEF index. */
	for (size_t i = 1; i < ehdr->e_shnum && i < SHN_LORESERVE; i++) {
		const Elf64_Shdr *shdr = elf_read_section_field(buf, ehdr, i);

		if (section_type == shdr->sh_type)
			elf_parse_symtab(buf, shdr, str_index, syms);
	}
}

/* Determines symbol section(s) and read symbols. */
void ujpp_elf_parse_mem(const char *buf, struct vector *v)
{
	/* Indexes of a string tables for .symtab and .dynsym sections. */
	size_t strtab_idx = elf_get_strsect_offset(buf, ".strtab");
	size_t dynstr_idx = elf_get_strsect_offset(buf, ".dynstr");

	elf_iterate_sections(buf, v, SHT_SYMTAB, strtab_idx);
	elf_iterate_sections(buf, v, SHT_DYNSYM, dynstr_idx);
}

/* Frees allocated memory. */
static void elf_terminate(char *buf, size_t size)
{
	ujpp_utils_unmap_file((void *)buf, size);
}

/* Reads symbol table from the file and saves it to the vector. */
void ujpp_elf_parse_file(const char *path, struct vector *v)
{
	/* Holds readed file. */
	size_t fsize;
	const char *buf = ujpp_utils_map_file(path, &fsize);

	if (NULL == buf)
		return;

	elf_check_file(buf);
	ujpp_elf_parse_mem(buf, v);
	elf_terminate((char *)buf, fsize);
}

static int elf_is_code_seg(const Elf64_Phdr *phdr)
{
	return (phdr->p_type == PT_LOAD) &&
	       ((phdr->p_flags & (PF_R | PF_X)) == (PF_R | PF_X));
}

size_t ujpp_elf_text_sz(const char *path)
{
	size_t fsz;
	size_t text_sz = 0;
	const char *buf = ujpp_utils_map_file(path, &fsz);
	const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)buf;

	elf_check_file(buf);

	for (size_t i = 0; i < ehdr->e_phnum; i++) {
		const Elf64_Phdr *phdr = elf_read_header_field(buf, ehdr, i);

		if (!elf_is_code_seg(phdr))
			continue;
		text_sz = phdr->p_memsz;

		if (phdr->p_vaddr != 0)
			text_sz += phdr->p_vaddr;
		break;
	}

	if (!text_sz)
		ujpp_utils_die("can't locate executable segment", NULL);

	ujpp_utils_unmap_file((void *)buf, fsz);
	return text_sz;
}
