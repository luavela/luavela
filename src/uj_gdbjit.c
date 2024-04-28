/*
 * Client for the GDB JIT API.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"

#if LJ_HASJIT

#include "uj_cframe.h"
#include "uj_dispatch.h"
#include "uj_proto.h"

#include "utils/leb128.h"

#ifdef GDBJIT

#include <elf.h>

#ifdef UJIT_IS_THREAD_SAFE
#include <pthread.h>

pthread_mutex_t __jit_debug_descriptor_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_JIT_DBG_DESCRIPTOR() \
	pthread_mutex_lock(&__jit_debug_descriptor_mutex)
#define UNLOCK_JIT_DBG_DESCRIPTOR() \
	pthread_mutex_unlock(&__jit_debug_descriptor_mutex)

#else /* UJIT_IS_THREAD_SAFE */

#define LOCK_JIT_DBG_DESCRIPTOR()
#define UNLOCK_JIT_DBG_DESCRIPTOR()

#endif /* UJIT_IS_THREAD_SAFE */

/* DWARF definitions. */
#define DW_CIE_VERSION 1

/*
 * The GDB JIT API allows JIT compilers to pass debug information about
 * JIT-compiled code back to GDB. You need at least GDB 7.0 or higher
 * to see it in action.
 *
 * This is a passive API, so it works even when not running under GDB
 * or when attaching to an already running process. Alas, this implies
 * enabling it always has a non-negligible overhead -- do not use in
 * release mode!
 *
 * The LuaJIT GDB JIT client is rather minimal at the moment. It gives
 * each trace a symbol name and adds a source location and frame unwind
 * information. Obviously LuaJIT itself and any embedding C application
 * should be compiled with debug symbols, too (see the Makefile).
 *
 * Traces are named TRACE_1, TRACE_2, ... these correspond to the trace
 * numbers from -jv or -jdump. Use "break TRACE_1" or "tbreak TRACE_1" etc.
 * to set breakpoints on specific traces (even ahead of their creation).
 *
 * The source location for each trace allows listing the corresponding
 * source lines with the GDB command "list" (but only if the Lua source
 * has been loaded from a file). Currently this is always set to the
 * location where the trace has been started.
 *
 * Frame unwind information can be inspected with the GDB command
 * "info frame". This also allows proper backtraces across JIT-compiled
 * code with the GDB command "bt".
 *
 * You probably want to add the following settings to a .gdbinit file
 * (or add them to ~/.gdbinit):
 *   set disassembly-flavor intel
 *   set breakpoint pending on
 *
 * Here's a sample GDB session:
 * ------------------------------------------------------------------------

$ cat >x.lua
for outer=1,100 do
  for inner=1,100 do end
end
^D

$ luajit -jv x.lua
[TRACE   1 x.lua:2]
[TRACE   2 (1/3) x.lua:1 -> 1]

$ gdb --quiet --args luajit x.lua
(gdb) tbreak TRACE_1
Function "TRACE_1" not defined.
Temporary breakpoint 1 (TRACE_1) pending.
(gdb) run
Starting program: luajit x.lua

Temporary breakpoint 1, TRACE_1 () at x.lua:2
2         for inner=1,100 do end
(gdb) list
1       for outer=1,100 do
2         for inner=1,100 do end
3       end
(gdb) bt
#0  TRACE_1 () at x.lua:2
#1  0x08053690 in lua_pcall [...]
[...]
#7  0x0806ff90 in main [...]
(gdb) disass TRACE_1
Dump of assembler code for function TRACE_1:
0xf7fd9fba <TRACE_1+0>: mov    DWORD PTR ds:0xf7e0e2a0,0x1
0xf7fd9fc4 <TRACE_1+10>:        movsd  xmm7,QWORD PTR [edx+0x20]
[...]
0xf7fd9ff8 <TRACE_1+62>:        jmp    0xf7fd2014
End of assembler dump.
(gdb) tbreak TRACE_2
Function "TRACE_2" not defined.
Temporary breakpoint 2 (TRACE_2) pending.
(gdb) cont
Continuing.

Temporary breakpoint 2, TRACE_2 () at x.lua:1
1       for outer=1,100 do
(gdb) info frame
Stack level 0, frame at 0xffffd7c0:
 eip = 0xf7fd9f60 in TRACE_2 (x.lua:1); saved eip 0x8053690
 called by frame at 0xffffd7e0
 source language unknown.
 Arglist at 0xffffd78c, args:
 Locals at 0xffffd78c, Previous frame's sp is 0xffffd7c0
 Saved registers:
  ebx at 0xffffd7ac, ebp at 0xffffd7b8, esi at 0xffffd7b0, edi at 0xffffd7b4,
  eip at 0xffffd7bc
(gdb)

 ** ------------------------------------------------------------------------
 */

/* -- GDB JIT API --------------------------------------------------------- */

/* GDB JIT actions. */
enum { GDBJIT_NOACTION = 0, GDBJIT_REGISTER, GDBJIT_UNREGISTER };

/* GDB JIT entry. */
struct gdb_jit_entry {
	struct gdb_jit_entry *next_entry;
	struct gdb_jit_entry *prev_entry;
	const char *symfile_addr;
	uint64_t symfile_size;
};

/* GDB JIT descriptor. */
struct gdb_jit_desc {
	uint32_t version;
	uint32_t action_flag;
	struct gdb_jit_entry *relevant_entry;
	struct gdb_jit_entry *first_entry;
};

struct gdb_jit_desc __jit_debug_descriptor = {1, GDBJIT_NOACTION, NULL, NULL};

/* GDB sets a breakpoint at this function. */
void LJ_NOINLINE __jit_debug_register_code(void)
{
	__asm__ __volatile__("");
}

/* -- In-memory ELF object definitions ------------------------------------ */

enum {
	DW_CFA_nop = 0x0,
	DW_CFA_offset_extended = 0x5,
	DW_CFA_def_cfa = 0xc,
	DW_CFA_def_cfa_offset = 0xe,
	DW_CFA_offset_extended_sf = 0x11,
	DW_CFA_advance_loc = 0x40,
	DW_CFA_offset = 0x80
};

enum { DW_EH_PE_udata4 = 3, DW_EH_PE_textrel = 0x20 };

enum { DW_TAG_compile_unit = 0x11 };

enum { DW_children_no = 0, DW_children_yes = 1 };

enum {
	DW_AT_name = 0x03,
	DW_AT_stmt_list = 0x10,
	DW_AT_low_pc = 0x11,
	DW_AT_high_pc = 0x12
};

enum { DW_FORM_addr = 0x01, DW_FORM_data4 = 0x06, DW_FORM_string = 0x08 };

enum {
	DW_LNS_extended_op = 0,
	DW_LNS_copy = 1,
	DW_LNS_advance_pc = 2,
	DW_LNS_advance_line = 3
};

enum { DW_LNE_end_sequence = 1, DW_LNE_set_address = 2 };

enum {
	/* Yes, the order is strange, but correct. */
	DW_REG_AX,
	DW_REG_DX,
	DW_REG_CX,
	DW_REG_BX,
	DW_REG_SI,
	DW_REG_DI,
	DW_REG_BP,
	DW_REG_SP,
	DW_REG_8,
	DW_REG_9,
	DW_REG_10,
	DW_REG_11,
	DW_REG_12,
	DW_REG_13,
	DW_REG_14,
	DW_REG_15,
	DW_REG_RA
};

/* Minimal list of sections for the in-memory ELF object. */
enum {
	GDBJIT_SECT_NULL,
	GDBJIT_SECT_text,
	GDBJIT_SECT_eh_frame,
	GDBJIT_SECT_shstrtab,
	GDBJIT_SECT_strtab,
	GDBJIT_SECT_symtab,
	GDBJIT_SECT_debug_info,
	GDBJIT_SECT_debug_abbrev,
	GDBJIT_SECT_debug_line,
	GDBJIT_SECT__MAX
};

enum { GDBJIT_SYM_UNDEF, GDBJIT_SYM_FILE, GDBJIT_SYM_FUNC, GDBJIT_SYM__MAX };

/* In-memory ELF object. */
struct gdb_jit_obj {
	Elf64_Ehdr hdr; /* ELF header. */
	Elf64_Shdr sect[GDBJIT_SECT__MAX]; /* ELF sections. */
	Elf64_Sym sym[GDBJIT_SYM__MAX]; /* ELF symbol table. */
	uint8_t space[4096]; /* Space for various section data. */
};

/* Combined structure for GDB JIT entry and ELF object. */
struct gdb_jit_entry_obj {
	struct gdb_jit_entry entry;
	size_t sz;
	struct gdb_jit_obj obj;
};

/* Template for in-memory ELF header. */
static const Elf64_Ehdr elfhdr_template = {
	.e_ident = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS64, ELFDATA2LSB,
		    EV_CURRENT, ELFOSABI_SYSV, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		    0x0, 0x0},
	.e_type = 1,
	.e_machine = 62,
	.e_version = 1,
	.e_entry = 0,
	.e_phoff = 0,
	.e_shoff = offsetof(struct gdb_jit_obj, sect),
	.e_flags = 0,
	.e_ehsize = sizeof(Elf64_Ehdr),
	.e_phentsize = 0,
	.e_phnum = 0,
	.e_shentsize = sizeof(Elf64_Shdr),
	.e_shnum = GDBJIT_SECT__MAX,
	.e_shstrndx = GDBJIT_SECT_shstrtab};

/* -- In-memory ELF object generation ------------------------------------- */

/* Context for generating the ELF object for the GDB JIT API. */
struct gdb_jit_ctx {
	uint8_t *p; /* Pointer to next address in obj.space. */
	uint8_t *startp; /* Pointer to start address in obj.space. */
	GCtrace *T; /* Generate symbols for this trace. */
	uintptr_t mcaddr; /* Machine code address. */
	size_t szmcode; /* Size of machine code. */
	size_t spadjp; /* Stack adjustment for parent
			   * trace or interpreter.
			   */
	size_t spadj; /* Stack adjustment for trace itself. */
	BCLine lineno; /* Starting line number. */
	const char *filename; /* Starting file name. */
	size_t objsize; /* Final size of ELF object. */
	struct gdb_jit_obj obj; /* In-memory ELF object. */
};

/* Add a zero-terminated string. */
static uint32_t gdbjit_strz(struct gdb_jit_ctx *ctx, const char *str)
{
	uint8_t *p = ctx->p;
	uint32_t ofs = (uint32_t)(p - ctx->startp);

	do {
		*p++ = (uint8_t)*str;
	} while (*str++);

	ctx->p = p;
	return ofs;
}

/* Append a decimal number. */
static void gdbjit_catnum(struct gdb_jit_ctx *ctx, uint32_t n)
{
	if (n >= 10) {
		uint32_t m = n / 10;

		n = n % 10;
		gdbjit_catnum(ctx, m);
	}
	*ctx->p++ = '0' + n;
}

/* Add a ULEB128 value. */
static void gdbjit_uleb128(struct gdb_jit_ctx *ctx, uint32_t v)
{
	ctx->p += write_uleb128(ctx->p, v);
}

/* Add a SLEB128 value. */
static void gdbjit_sleb128(struct gdb_jit_ctx *ctx, int32_t v)
{
	ctx->p += write_leb128(ctx->p, v);
}

/* Functions to generate DWARF structures. */
static void DB(uint8_t **p, uint8_t x)
{
	*(*p) = x;
	(*p) += sizeof(x);
}

static void DI8(uint8_t **p, int8_t x)
{
	*(int8_t *)(*p) = x;
	(*p) += sizeof(x);
}

static void DU16(uint8_t **p, uint16_t x)
{
	*(uint16_t *)(*p) = x;
	(*p) += sizeof(x);
}

static void DU32(uint8_t **p, uint32_t x)
{
	*(uint32_t *)(*p) = x;
	(*p) += sizeof(x);
}

static void DADDR(uint8_t **p, uintptr_t x)
{
	*(uintptr_t *)(*p) = x;
	(*p) += sizeof(uintptr_t);
}

static void DUV(uint8_t **p, struct gdb_jit_ctx *ctx, uint32_t x)
{
	ctx->p = *p;
	gdbjit_uleb128(ctx, x);
	*p = ctx->p;
}

static void DSV(uint8_t **p, struct gdb_jit_ctx *ctx, int32_t x)
{
	ctx->p = *p;
	gdbjit_sleb128(ctx, x);
	*p = ctx->p;
}

static void DSTR(uint8_t **p, struct gdb_jit_ctx *ctx, const char *str)
{
	ctx->p = *p;
	gdbjit_strz(ctx, str);
	*p = ctx->p;
}

static void DALIGNNOP(uint8_t **p, size_t s)
{
	while ((uintptr_t)*p & (s - 1)) {
		*(*p) = DW_CFA_nop;
		(*p)++;
	}
}

static Elf64_Shdr *sectdef(struct gdb_jit_ctx *ctx, int32_t id,
			   const char *name, Elf64_Word type, Elf64_Xword align)
{
	Elf64_Shdr *sect = &ctx->obj.sect[id];

	sect->sh_name = gdbjit_strz(ctx, name);
	sect->sh_type = type;
	sect->sh_addralign = align;
	return sect;
}

/* Initialize ELF section headers. */
static void gdbjit_secthdr(struct gdb_jit_ctx *ctx)
{
	Elf64_Shdr *sect;

	*ctx->p++ = '\0'; /* Empty string at start of string table. */

	sect = sectdef(ctx, GDBJIT_SECT_text, ".text", SHT_NOBITS, 16);
	sect->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	sect->sh_addr = ctx->mcaddr;
	sect->sh_offset = 0;
	sect->sh_size = ctx->szmcode;

	sect = sectdef(ctx, GDBJIT_SECT_eh_frame, ".eh_frame", SHT_PROGBITS,
		       sizeof(uintptr_t));
	sect->sh_flags = SHF_ALLOC;

	sectdef(ctx, GDBJIT_SECT_shstrtab, ".shstrtab", SHT_STRTAB, 1);

	sectdef(ctx, GDBJIT_SECT_strtab, ".strtab", SHT_STRTAB, 1);

	sect = sectdef(ctx, GDBJIT_SECT_symtab, ".symtab", SHT_SYMTAB,
		       sizeof(uintptr_t));
	sect->sh_offset = offsetof(struct gdb_jit_obj, sym);
	sect->sh_size = sizeof(ctx->obj.sym);
	sect->sh_link = GDBJIT_SECT_strtab;
	sect->sh_entsize = sizeof(Elf64_Sym);
	sect->sh_info = GDBJIT_SYM_FUNC;

	sectdef(ctx, GDBJIT_SECT_debug_info, ".debug_info", SHT_PROGBITS, 1);

	sectdef(ctx, GDBJIT_SECT_debug_abbrev, ".debug_abbrev", SHT_PROGBITS,
		1);

	sectdef(ctx, GDBJIT_SECT_debug_line, ".debug_line", SHT_PROGBITS, 1);
}

/* Initialize symbol table. */
static void gdbjit_symtab(struct gdb_jit_ctx *ctx)
{
	Elf64_Sym *sym;

	*ctx->p++ = '\0'; /* Empty string at start of string table. */

	sym = &ctx->obj.sym[GDBJIT_SYM_FILE];
	sym->st_name = gdbjit_strz(ctx, "JIT mcode");
	sym->st_shndx = SHN_ABS;
	sym->st_info = STT_FILE | (STB_LOCAL << 4);

	sym = &ctx->obj.sym[GDBJIT_SYM_FUNC];
	sym->st_name = gdbjit_strz(ctx, "TRACE_");
	ctx->p--;
	gdbjit_catnum(ctx, ctx->T->traceno);
	*ctx->p++ = '\0';
	sym->st_shndx = GDBJIT_SECT_text;
	sym->st_value = 0;
	sym->st_size = ctx->szmcode;
	sym->st_info = STT_FUNC | (STB_GLOBAL << 4);
}

/* Initialize .eh_frame section. */
static void gdbjit_ehframe(struct gdb_jit_ctx *ctx)
{
	uint8_t *p = ctx->p;
	uint8_t *framep = p;
	uint32_t *szp_CIE;
	uint32_t *szp_FDE;

	/* Emit DWARF EH CIE. */
	szp_CIE = (uint32_t *)p;
	p += 4;
	/* Offset to CIE itself. */
	DU32(&p, 0);
	DB(&p, DW_CIE_VERSION);
	/* Augmentation. */
	DSTR(&p, ctx, "zR");
	/* Code alignment factor. */
	DUV(&p, ctx, 1);
	/* Data alignment factor. */
	DSV(&p, ctx, -(int32_t)sizeof(uintptr_t));
	/* Return address register. */
	DB(&p, DW_REG_RA);
	/* Augmentation data. */
	DB(&p, 1);
	DB(&p, DW_EH_PE_textrel | DW_EH_PE_udata4);
	DB(&p, DW_CFA_def_cfa);
	DUV(&p, ctx, DW_REG_SP);
	DUV(&p, ctx, sizeof(uintptr_t));
	DB(&p, DW_CFA_offset | DW_REG_RA);
	DUV(&p, ctx, 1);
	DALIGNNOP(&p, sizeof(uintptr_t));
	*szp_CIE = (uint32_t)((p - (uint8_t *)szp_CIE) - 4);

	/* Emit DWARF EH FDE. */
	szp_FDE = (uint32_t *)p;
	p += 4;
	/* Offset to CIE. */
	DU32(&p, (uint32_t)(p - framep));
	/* Machine code offset relative to .text. */
	DU32(&p, 0);
	/* Machine code length. */
	DU32(&p, ctx->szmcode);
	/* Augmentation data. */
	DB(&p, 0);
	/* Registers saved in CFRAME. */
	DB(&p, DW_CFA_offset | DW_REG_BP);
	DUV(&p, ctx, 2);
	DB(&p, DW_CFA_offset | DW_REG_BX);
	DUV(&p, ctx, 3);
	DB(&p, DW_CFA_offset | DW_REG_15);
	DUV(&p, ctx, 4);
	DB(&p, DW_CFA_offset | DW_REG_14);
	DUV(&p, ctx, 5);
	/* Extra registers saved for JIT-compiled code. */
	DB(&p, DW_CFA_offset | DW_REG_13);
	DUV(&p, ctx, 9);
	DB(&p, DW_CFA_offset | DW_REG_12);
	DUV(&p, ctx, 10);

	/* Parent/interpreter stack frame size. */
	if (ctx->spadjp != ctx->spadj) {
		DB(&p, DW_CFA_def_cfa_offset);
		DUV(&p, ctx, ctx->spadjp);
		/* Only an approximation. */
		DB(&p, DW_CFA_advance_loc | 1);
	}

	/* Trace stack frame size. */
	DB(&p, DW_CFA_def_cfa_offset);
	DUV(&p, ctx, ctx->spadj);
	DALIGNNOP(&p, sizeof(uintptr_t));
	*szp_FDE = (uint32_t)((p - (uint8_t *)szp_FDE) - 4);

	ctx->p = p;
}

/* Initialize .debug_info section. */
static void gdbjit_debuginfo(struct gdb_jit_ctx *ctx)
{
	uint8_t *p = ctx->p;
	uint32_t *szp_info = (uint32_t *)p;

	p += sizeof(*szp_info);
	/* DWARF version. */
	DU16(&p, 2);
	/* Abbrev offset. */
	DU32(&p, 0);
	/* Pointer size. */
	DB(&p, sizeof(uintptr_t));
	/* Abbrev #1: DW_TAG_compile_unit.*/
	DUV(&p, ctx, 1);
	/* DW_AT_name. */
	DSTR(&p, ctx, ctx->filename);
	/* DW_AT_low_pc. */
	DADDR(&p, ctx->mcaddr);
	/* DW_AT_high_pc. */
	DADDR(&p, ctx->mcaddr + ctx->szmcode);
	/* DW_AT_stmt_list. */
	DU32(&p, 0);
	*szp_info = (uint32_t)((p - (uint8_t *)szp_info) - 4);

	ctx->p = p;
}

/* Initialize .debug_abbrev section. */
static void gdbjit_debugabbrev(struct gdb_jit_ctx *ctx)
{
	uint8_t *p = ctx->p;

	/* Abbrev #1: DW_TAG_compile_unit. */
	DUV(&p, ctx, 1);
	DUV(&p, ctx, DW_TAG_compile_unit);
	DB(&p, DW_children_no);
	DUV(&p, ctx, DW_AT_name);
	DUV(&p, ctx, DW_FORM_string);
	DUV(&p, ctx, DW_AT_low_pc);
	DUV(&p, ctx, DW_FORM_addr);
	DUV(&p, ctx, DW_AT_high_pc);
	DUV(&p, ctx, DW_FORM_addr);
	DUV(&p, ctx, DW_AT_stmt_list);
	DUV(&p, ctx, DW_FORM_data4);
	DB(&p, 0);
	DB(&p, 0);

	ctx->p = p;
}

static void DLNE(uint8_t **p, struct gdb_jit_ctx *ctx, int8_t op, uint32_t s)
{
	DB(p, DW_LNS_extended_op);
	DUV(p, ctx, 1 + s);
	DB(p, op);
}

/* Initialize .debug_line section. */
static void gdbjit_debugline(struct gdb_jit_ctx *ctx)
{
	uint8_t *p = ctx->p;
	uint32_t *szp_line;
	uint32_t *szp_header;

	szp_line = (uint32_t *)p;
	p += sizeof(*szp_line);
	/* DWARF version. */
	DU16(&p, 2);
	szp_header = (uint32_t *)p;
	p += sizeof(*szp_header);
	/* Minimum instruction length. */
	DB(&p, 1);
	/* is_stmt. */
	DB(&p, 1);
	/* Line base for special opcodes. */
	DI8(&p, 0);
	/* Line range for special opcodes. */
	DB(&p, 2);
	/* Opcode base at DW_LNS_advance_line+1. */
	DB(&p, 3 + 1);
	/* Standard opcode lengths. */
	DB(&p, 0);
	DB(&p, 1);
	DB(&p, 1);
	/* Directory table. */
	DB(&p, 0);
	/* File name table. */
	DSTR(&p, ctx, ctx->filename);
	DUV(&p, ctx, 0);
	DUV(&p, ctx, 0);
	DUV(&p, ctx, 0);
	DB(&p, 0);
	*szp_header = (uint32_t)((p - (uint8_t *)szp_header) - 4);

	DLNE(&p, ctx, DW_LNE_set_address, sizeof(uintptr_t));
	DADDR(&p, ctx->mcaddr);

	if (ctx->lineno) {
		DB(&p, DW_LNS_advance_line);
		DSV(&p, ctx, ctx->lineno - 1);
	}

	DB(&p, DW_LNS_copy);
	DB(&p, DW_LNS_advance_pc);
	DUV(&p, ctx, ctx->szmcode);
	DLNE(&p, ctx, DW_LNE_end_sequence, 0);
	*szp_line = (uint32_t)((p - (uint8_t *)szp_line) - 4);

	ctx->p = p;
}

/* Type of a section initializer callback. */
typedef void (*GDBJITinitf)(struct gdb_jit_ctx *ctx);

/* Call section initializer and set the section offset and size. */
static void gdbjit_initsect(struct gdb_jit_ctx *ctx, int sect,
			    GDBJITinitf initf)
{
	ctx->startp = ctx->p;
	ctx->obj.sect[sect].sh_offset =
		(uintptr_t)((char *)ctx->p - (char *)&ctx->obj);
	initf(ctx);
	ctx->obj.sect[sect].sh_size = (uintptr_t)(ctx->p - ctx->startp);
}

uint8_t *sect_align(uint8_t *p, size_t a)
{
	return (uint8_t *)(((uintptr_t)p + (a - 1)) & ~(uintptr_t)(a - 1));
}

/* Build in-memory ELF object. */
static void gdbjit_buildobj(struct gdb_jit_ctx *ctx)
{
	struct gdb_jit_obj *obj = &ctx->obj;
	/* Fill in ELF header and clear structures. */
	memcpy(&obj->hdr, &elfhdr_template, sizeof(Elf64_Ehdr));
	memset(&obj->sect, 0, sizeof(Elf64_Shdr) * GDBJIT_SECT__MAX);
	memset(&obj->sym, 0, sizeof(Elf64_Sym) * GDBJIT_SYM__MAX);
	/* Initialize sections. */
	ctx->p = obj->space;
	gdbjit_initsect(ctx, GDBJIT_SECT_shstrtab, gdbjit_secthdr);
	gdbjit_initsect(ctx, GDBJIT_SECT_strtab, gdbjit_symtab);
	gdbjit_initsect(ctx, GDBJIT_SECT_debug_info, gdbjit_debuginfo);
	gdbjit_initsect(ctx, GDBJIT_SECT_debug_abbrev, gdbjit_debugabbrev);
	gdbjit_initsect(ctx, GDBJIT_SECT_debug_line, gdbjit_debugline);
	ctx->p = sect_align(ctx->p, sizeof(uintptr_t));
	gdbjit_initsect(ctx, GDBJIT_SECT_eh_frame, gdbjit_ehframe);
	ctx->objsize = (size_t)((char *)ctx->p - (char *)obj);
	lua_assert(ctx->objsize < sizeof(struct gdb_jit_obj));
}

/* -- Interface to GDB JIT API -------------------------------------------- */

/* Add new entry to GDB JIT symbol chain. */
static void gdbjit_newentry(lua_State *L, struct gdb_jit_ctx *ctx)
{
	/* Allocate memory for GDB JIT entry and ELF object. */
	size_t sz = sizeof(struct gdb_jit_entry_obj) -
		    sizeof(struct gdb_jit_obj) + ctx->objsize;
	struct gdb_jit_entry_obj *eo = uj_mem_alloc(L, sz);

	memcpy(&eo->obj, &ctx->obj, ctx->objsize); /* Copy ELF object. */
	eo->sz = sz;
	ctx->T->gdbjit_entry = (void *)eo;
	/* Link new entry to chain and register it. */
	eo->entry.prev_entry = NULL;

	LOCK_JIT_DBG_DESCRIPTOR();

	eo->entry.next_entry = __jit_debug_descriptor.first_entry;
	if (eo->entry.next_entry)
		eo->entry.next_entry->prev_entry = &eo->entry;
	eo->entry.symfile_addr = (const char *)&eo->obj;
	eo->entry.symfile_size = ctx->objsize;
	__jit_debug_descriptor.first_entry = &eo->entry;
	__jit_debug_descriptor.relevant_entry = &eo->entry;
	__jit_debug_descriptor.action_flag = GDBJIT_REGISTER;
	__jit_debug_register_code();

	UNLOCK_JIT_DBG_DESCRIPTOR();
}

/* Add debug info for newly compiled trace and notify GDB. */
void uj_gdbjit_addtrace(const jit_State *J, GCtrace *T)
{
	struct gdb_jit_ctx ctx;
	const GCproto *pt = T->startpt;
	TraceNo parent = T->ir[REF_BASE].op1;
	const BCIns *startpc = T->startpc;

	ctx.T = T;
	ctx.mcaddr = (uintptr_t)T->mcode;
	ctx.szmcode = T->szmcode;
	ctx.spadjp = CFRAME_SIZE_JIT +
		     (size_t)(parent ? traceref(J, parent)->spadjust : 0);
	ctx.spadj = CFRAME_SIZE_JIT + T->spadjust;
	lua_assert(startpc >= proto_bc(pt) &&
		   startpc < proto_bc(pt) + pt->sizebc);
	ctx.lineno = uj_proto_line(pt, proto_bcpos(pt, startpc));
	ctx.filename = proto_chunknamestr(pt);
	if (*ctx.filename == '@' || *ctx.filename == '=')
		ctx.filename++;
	else
		ctx.filename = "(string)";
	gdbjit_buildobj(&ctx);
	gdbjit_newentry(J->L, &ctx);
}

/* Delete debug info for trace and notify GDB. */
void uj_gdbjit_deltrace(const jit_State *J, GCtrace *T)
{
	struct gdb_jit_entry_obj *eo =
		(struct gdb_jit_entry_obj *)T->gdbjit_entry;

	if (!eo)
		return;

	LOCK_JIT_DBG_DESCRIPTOR();

	if (eo->entry.prev_entry)
		eo->entry.prev_entry->next_entry = eo->entry.next_entry;
	else
		__jit_debug_descriptor.first_entry = eo->entry.next_entry;

	if (eo->entry.next_entry)
		eo->entry.next_entry->prev_entry = eo->entry.prev_entry;

	__jit_debug_descriptor.relevant_entry = &eo->entry;
	__jit_debug_descriptor.action_flag = GDBJIT_UNREGISTER;
	__jit_debug_register_code();

	UNLOCK_JIT_DBG_DESCRIPTOR();

	uj_mem_free(MEM_G(J2G(J)), eo, eo->sz);
}

#endif /* GDBJIT */

#endif /* LJ_HASJIT */
