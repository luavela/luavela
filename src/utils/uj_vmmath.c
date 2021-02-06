/*
 * Math helper functions for assembler VM.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <math.h>
#include "lj_def.h"

/*
 * Helper functions for VM to avoid linker issues since *.dasc files don't
 * support calls to dynamically linked functions (i.e. to libm, libc, etc.)
 * They are not declared in any header to prohibit calling them from C code.
 */

double uj_vm_asin(double x)
{
	return asin(x);
}

double uj_vm_acos(double x)
{
	return acos(x);
}

double uj_vm_atan(double x)
{
	return atan(x);
}

double uj_vm_sinh(double x)
{
	return sinh(x);
}

double uj_vm_cosh(double x)
{
	return cosh(x);
}

double uj_vm_tanh(double x)
{
	return tanh(x);
}

double uj_vm_floor(double x)
{
	return floor(x);
}

double uj_vm_ceil(double x)
{
	return ceil(x);
}

double uj_vm_trunc(double x)
{
	return trunc(x);
}

double uj_vm_exp(double x)
{
	return exp(x);
}

double uj_vm_modf(double x, double *iptr)
{
	return modf(x, iptr);
}

double uj_vm_frexp(double x, int *exp)
{
	return frexp(x, exp);
}

double uj_vm_pow(double x, double exp)
{
	return pow(x, exp);
}

double uj_vm_mod(double a, double b)
{
	/* according to Lua Reference */
	return a - floor(a / b) * b;
}
