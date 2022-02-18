/*
 * Machine-dependent optimisations for x86/x64.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _OPT_X86_IFACE_H
#define _OPT_X86_IFACE_H

struct ASMState;

void uj_opt_x86_fold_test_rr(struct ASMState *as);

#endif /* !_OPT_X86_IFACE_H */
