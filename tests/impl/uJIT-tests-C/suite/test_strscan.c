/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "test_common.h"

#include <utils/strscan.c>

#define ASSERT_STRSCAN_GOOD_CONV_TO_NUM(s, expected)                          \
	do {                                                                  \
		StrScanFmt tmp_format;                                        \
		double tmp_value;                                             \
		tmp_format = strscan_tonumber((const uint8_t *)(s),           \
					      &tmp_value, STRSCAN_OPT_TONUM); \
		assert_int_equal(tmp_format, STRSCAN_NUM);                    \
		assert_true(tmp_value == (expected));                         \
	} while (0)

#define ASSERT_STRSCAN_GOOD_CONV_TO_I32(s, expected)                         \
	do {                                                                 \
		StrScanFmt tmp_format;                                       \
		FpConv tmp_conv;                                             \
		tmp_format = strscan_tonumber((const uint8_t *)(s),          \
					      &(tmp_conv.d), STRSCAN_OPT_C); \
		assert_int_equal(tmp_format, STRSCAN_INT);                   \
		assert_true(tmp_conv.i == (expected));                       \
	} while (0)

#define ASSERT_STRSCAN_GOOD_CONV_TO_U32(s, expected)                         \
	do {                                                                 \
		StrScanFmt tmp_format;                                       \
		FpConv tmp_conv;                                             \
		tmp_format = strscan_tonumber((const uint8_t *)(s),          \
					      &(tmp_conv.d), STRSCAN_OPT_C); \
		assert_int_equal(tmp_format, STRSCAN_U32);                   \
		assert_true(tmp_conv.lo == (expected));                      \
	} while (0)

#define ASSERT_STRSCAN_GOOD_CONV_TO_I64(s, expected)                           \
	do {                                                                   \
		StrScanFmt tmp_format;                                         \
		FpConv tmp_conv;                                               \
		tmp_format = strscan_tonumber((const uint8_t *)(s),            \
					      &(tmp_conv.d),                   \
					      STRSCAN_OPT_C | STRSCAN_OPT_LL); \
		assert_int_equal(tmp_format, STRSCAN_I64);                     \
		assert_true((int64_t)(tmp_conv.u) == (expected));              \
	} while (0)

#define ASSERT_STRSCAN_GOOD_CONV_TO_U64(s, expected)                           \
	do {                                                                   \
		StrScanFmt tmp_format;                                         \
		FpConv tmp_conv;                                               \
		tmp_format = strscan_tonumber((const uint8_t *)(s),            \
					      &(tmp_conv.d),                   \
					      STRSCAN_OPT_C | STRSCAN_OPT_LL); \
		assert_int_equal(tmp_format, STRSCAN_U64);                     \
		assert_true(tmp_conv.u == (expected));                         \
	} while (0)

#define ASSERT_STRSCAN_GOOD_CONV_TO_IMAG(s, expected)                        \
	do {                                                                 \
		StrScanFmt tmp_format;                                       \
		double tmp_value;                                            \
		tmp_format = strscan_tonumber((const uint8_t *)(s),          \
					      &tmp_value, STRSCAN_OPT_IMAG); \
		assert_int_equal(tmp_format, STRSCAN_IMAG);                  \
		assert_true(tmp_value == (expected));                        \
	} while (0)

#define ASSERT_STRSCAN_FAILURE(s, opt)                              \
	do {                                                        \
		StrScanFmt tmp_format;                              \
		double tmp_value;                                   \
		tmp_format = strscan_tonumber((const uint8_t *)(s), \
					      &tmp_value, (opt));   \
		assert_int_equal(tmp_format, STRSCAN_ERROR);        \
	} while (0)

#define ASSERT_STRSCAN_PINF(s)                                      \
	do {                                                        \
		StrScanFmt tmp_format;                              \
		FpConv tmp_conv;                                    \
		tmp_format = strscan_tonumber((const uint8_t *)(s), \
					      &(tmp_conv.d),        \
					      STRSCAN_OPT_TONUM);   \
		assert_int_equal(tmp_format, STRSCAN_NUM);          \
		assert_true(tmp_conv.u == LJ_PINFINITY);            \
	} while (0)

#define ASSERT_STRSCAN_MINF(s)                                      \
	do {                                                        \
		StrScanFmt tmp_format;                              \
		FpConv tmp_conv;                                    \
		tmp_format = strscan_tonumber((const uint8_t *)(s), \
					      &(tmp_conv.d),        \
					      STRSCAN_OPT_TONUM);   \
		assert_int_equal(tmp_format, STRSCAN_NUM);          \
		assert_true(tmp_conv.u == LJ_MINFINITY);            \
	} while (0)

#define ASSERT_STRSCAN_NAN(s)                                       \
	do {                                                        \
		StrScanFmt tmp_format;                              \
		FpConv tmp_conv;                                    \
		tmp_format = strscan_tonumber((const uint8_t *)(s), \
					      &(tmp_conv.d),        \
					      STRSCAN_OPT_TONUM);   \
		assert_int_equal(tmp_format, STRSCAN_NUM);          \
		assert_true(tmp_conv.u == LJ_NAN);                  \
	} while (0)

static void test_decimal(void **state)
{
	UNUSED_STATE(state);

	StrScanFmt format;
	double value;
	FpConv conv;

	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("1000000000000", 1.0E12);

	/* Integers, decimal notation */
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("1230", 1230.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-1230", -1230.0);

	/* Floating point, decimal notation */
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("3.045", 3.045);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-3.045", -3.045);

	/*
	 * Exponential notation, integers: signs and significand/exponent
	 * delimiter
	 */
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("2E4", 2E4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("2e4", 2e4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("2E+4", 2E+4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("2e+4", 2e+4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-2E4", -2E4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-2e4", -2e4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("2E-4", 2E-4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("2e-4", 2e-4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-2E-4", -2E-4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-2e-4", -2e-4);

	/*
	 * Exponential notation, floating point: scaling
	 */

	format = strscan_tonumber((const uint8_t *)"-22.03e42", &value,
				  STRSCAN_OPT_TONUM);
	conv.d = value;
	assert_int_equal(format, STRSCAN_NUM);
	assert_true(conv.d == -2.203e43);
	assert_true(conv.u == 0xC8EF9C8B3E6F85C8);

	format = strscan_tonumber((const uint8_t *)"-22.03e-42", &value,
				  STRSCAN_OPT_TONUM);
	conv.d = value;
	assert_int_equal(format, STRSCAN_NUM);
	assert_true(conv.d == -2.203e-41);
	assert_true(conv.u == 0xB77EB49111205B88);

	/* Exponential notation, floating point: subnormal */
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0.4E-323", 0.4E-323);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0.4E-323", -0.4E-323);

	/* Exponential notation, floating point: below epsilon */
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0.4E-500", 0.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0.4E-500", 0.0);
}

static void test_hexadecimal(void **state)
{
	UNUSED_STATE(state);

	/* Integers, decimal notation */
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x1230", (double)0x1230);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x1230", (double)-0x1230);

	/* Floating point, decimal notation */
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x3.045", 0x3.045P0);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x3.045", -0x3.045P0);

	/*
	 * Exponential notation, integers: signs and significand/exponent
	 * delimiter
	 */

	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x2P4", 0x2P4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x2p4", 0x2p4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x2P+4", 0x2P+4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x2p+4", 0x2p+4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x2P4", -0x2P4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x2p4", -0x2p4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x2P-4", 0x2P-4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x2p-4", 0x2p-4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x2P-4", -0x2P-4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x2p-4", -0x2p-4);

	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x2E4", 0x2E4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x2e4", 0x2e4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x2E4", -0x2E4);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x2e4", -0x2e4);

	ASSERT_STRSCAN_FAILURE("0x2E+4", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("0x2e+4", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("0x2E-4", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("0x2e-4", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("-0x2E-4", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("-0x2e-4", STRSCAN_OPT_TONUM);

	/* Exponential notation, floating point: scaling */

	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x22.03p23", -0x2.203p27);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0x22.03p-23", -0x2.203p-19);

	/* Exponential notation, floating point: subnormal */

	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("+0xFF.Fp-1079", 0xFF.Fp-1079);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0xFF.Fp-1079", -0xFF.Fp-1079);

	/* Exponential notation, floating point: below epsilon */

	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("+0xF.Fp-1079", 0.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-0xF.Fp-1079", 0.0);
}

static void test_octal(void **state)
{
	/*
	 * Octal literals are limited: only integers, with or without suffices.
	 * So octals are tested mainly in test_suffices case.
	 */

	UNUSED_STATE(state);

	StrScanFmt format;
	double value;

	/* No floating point octal literals, '0' prefix is simply ignored */

	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("03.045", 3.045);
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("-03.045", -3.045);

	format = strscan_tonumber((const uint8_t *)"03.045", &value,
				  STRSCAN_OPT_C);
	assert_int_equal(format, STRSCAN_NUM);
	assert_true(value == 3.045);

	format = strscan_tonumber((const uint8_t *)"-03.045", &value,
				  STRSCAN_OPT_C);
	assert_int_equal(format, STRSCAN_NUM);
	assert_true(value == -3.045);
}

static void test_special_floats(void **state)
{
	UNUSED_STATE(state);

	ASSERT_STRSCAN_PINF("  inf ");
	ASSERT_STRSCAN_PINF(" +inf ");
	ASSERT_STRSCAN_PINF("  INF ");
	ASSERT_STRSCAN_PINF(" +INF ");
	ASSERT_STRSCAN_MINF(" -inf ");
	ASSERT_STRSCAN_MINF(" -INF ");

	ASSERT_STRSCAN_PINF("  infinity ");
	ASSERT_STRSCAN_PINF(" +infinity ");
	ASSERT_STRSCAN_PINF("  Infinity ");
	ASSERT_STRSCAN_PINF(" +Infinity ");
	ASSERT_STRSCAN_PINF("  INFINITY ");
	ASSERT_STRSCAN_PINF(" +INFINITY ");

	ASSERT_STRSCAN_MINF(" -infinity ");
	ASSERT_STRSCAN_MINF(" -Infinity ");
	ASSERT_STRSCAN_MINF(" -INFINITY ");

	ASSERT_STRSCAN_NAN("  nan ");
	ASSERT_STRSCAN_NAN(" +nan ");
	ASSERT_STRSCAN_NAN(" -nan ");
	ASSERT_STRSCAN_NAN("  NAN ");
	ASSERT_STRSCAN_NAN(" +NAN ");
	ASSERT_STRSCAN_NAN(" -NAN ");
	ASSERT_STRSCAN_NAN("  NaN ");
	ASSERT_STRSCAN_NAN(" +NaN ");
	ASSERT_STRSCAN_NAN(" -NaN ");
}

static void test_suffices(void **state)
{
	/*
	 * This is for 32- and 64-bit integers only, all bases are tested within
	 * this single case.
	 */

	UNUSED_STATE(state);

	/* NB! No suffix means int32_t */
	ASSERT_STRSCAN_GOOD_CONV_TO_I32("10", (int32_t)10);
	ASSERT_STRSCAN_GOOD_CONV_TO_I32("-10", (int32_t)-10);
	ASSERT_STRSCAN_GOOD_CONV_TO_I32("010", (int32_t)010);
	ASSERT_STRSCAN_GOOD_CONV_TO_I32("-010", (int32_t)-010);
	ASSERT_STRSCAN_GOOD_CONV_TO_I32("0x10", (int32_t)0x10);
	ASSERT_STRSCAN_GOOD_CONV_TO_I32("-0x10", (int32_t)-0x10);

	ASSERT_STRSCAN_GOOD_CONV_TO_U32("10U", 10U);
	ASSERT_STRSCAN_GOOD_CONV_TO_U32("10u", 10u);
	ASSERT_STRSCAN_GOOD_CONV_TO_U32("+10U", 10U);
	ASSERT_STRSCAN_GOOD_CONV_TO_U32("+10u", 10u);

	ASSERT_STRSCAN_GOOD_CONV_TO_U32("010U", 010U);
	ASSERT_STRSCAN_GOOD_CONV_TO_U32("010u", 010u);
	ASSERT_STRSCAN_GOOD_CONV_TO_U32("+010U", 010U);
	ASSERT_STRSCAN_GOOD_CONV_TO_U32("+010u", 010u);

	ASSERT_STRSCAN_GOOD_CONV_TO_U32("0x10U", 0x10U);
	ASSERT_STRSCAN_GOOD_CONV_TO_U32("0x10u", 0x10u);
	ASSERT_STRSCAN_GOOD_CONV_TO_U32("+0x10U", 0x10U);
	ASSERT_STRSCAN_GOOD_CONV_TO_U32("+0x10u", 0x10u);

	if (sizeof(long) == sizeof(int64_t)) {
		/*
		 * We are on x86_64, long is 64 bits, prefix L is automatically
		 * upgraded to LL inside strscan, so STRSCAN_OPT_LL is required:
		 */
		ASSERT_STRSCAN_FAILURE("-10L", STRSCAN_OPT_C);
		ASSERT_STRSCAN_FAILURE("-10l", STRSCAN_OPT_C);
		ASSERT_STRSCAN_FAILURE("-010L", STRSCAN_OPT_C);
		ASSERT_STRSCAN_FAILURE("-010l", STRSCAN_OPT_C);
		ASSERT_STRSCAN_FAILURE("-0x10L", STRSCAN_OPT_C);
		ASSERT_STRSCAN_FAILURE("-0x10l", STRSCAN_OPT_C);

		ASSERT_STRSCAN_GOOD_CONV_TO_I64("10L", 10L);
		ASSERT_STRSCAN_GOOD_CONV_TO_I64("10l", 10l);
		ASSERT_STRSCAN_GOOD_CONV_TO_I64("-10L", -10L);
		ASSERT_STRSCAN_GOOD_CONV_TO_I64("-10l", -10l);

		ASSERT_STRSCAN_GOOD_CONV_TO_I64("010L", 010L);
		ASSERT_STRSCAN_GOOD_CONV_TO_I64("010l", 010l);
		ASSERT_STRSCAN_GOOD_CONV_TO_I64("-010L", -010L);
		ASSERT_STRSCAN_GOOD_CONV_TO_I64("-010l", -010l);

		ASSERT_STRSCAN_GOOD_CONV_TO_I64("0x10L", 0x10L);
		ASSERT_STRSCAN_GOOD_CONV_TO_I64("0x10l", 0x10l);
		ASSERT_STRSCAN_GOOD_CONV_TO_I64("-0x10L", -0x10L);
		ASSERT_STRSCAN_GOOD_CONV_TO_I64("-0x10l", -0x10l);
	}

	ASSERT_STRSCAN_GOOD_CONV_TO_I64("10LL", 10LL);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("10ll", 10ll);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("-10LL", -10LL);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("-10ll", -10ll);

	ASSERT_STRSCAN_GOOD_CONV_TO_I64("010LL", 010LL);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("010ll", 010ll);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("-010LL", -010LL);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("-010ll", -010ll);

	ASSERT_STRSCAN_GOOD_CONV_TO_I64("0x10LL", 0x10LL);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("0x10ll", 0x10ll);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("-0x10LL", -0x10LL);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("-0x10ll", -0x10ll);

	ASSERT_STRSCAN_GOOD_CONV_TO_U64("10LLU", 10LLU);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("10llu", 10llu);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+10LLU", 10LLU);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+10llu", 10llu);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("10ULL", 10ULL);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("10ull", 10ull);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+10ULL", 10ULL);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+10ull", 10ull);

	ASSERT_STRSCAN_GOOD_CONV_TO_U64("010LLU", 010LLU);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("010llu", 010llu);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+010LLU", 010LLU);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+010llu", 010llu);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("010ULL", 010ULL);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("010ull", 010ull);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+010ULL", 010ULL);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+010ull", 010ull);

	ASSERT_STRSCAN_GOOD_CONV_TO_U64("0x10LLU", 0x10LLU);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("0x10llu", 0x10llu);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+0x10LLU", 0x10LLU);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+0x10llu", 0x10llu);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("0x10ULL", 0x10ULL);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("0x10ull", 0x10ull);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+0x10ULL", 0x10ULL);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+0x10ull", 0x10ull);

	/* NB! Mixed-case suffices, parsed by us, not allowed in C standard: */
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("-10Ll", -10LL);
	ASSERT_STRSCAN_GOOD_CONV_TO_I64("-10lL", -10LL);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+10uLl", 10ULL);
	ASSERT_STRSCAN_GOOD_CONV_TO_U64("+10ulL", 10ULL);
}

static void test_errors(void **state)
{
	UNUSED_STATE(state);

	ASSERT_STRSCAN_FAILURE("", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("     ", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE(" ### ", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("   - ", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("  -", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("   + ", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("  +", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("   +- ", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("   +-inf", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("  +- inf", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("  +infonity", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("  +NaNumber", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE(" +127.0.0.1", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("   0x0xfeed", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("0xfeed0xfeed", STRSCAN_OPT_TONUM);

	/* No suffix for hex: */
	ASSERT_STRSCAN_FAILURE("10A", STRSCAN_OPT_TONUM);

	/* Suffix for imaginary without corresponding opt is an error: */
	ASSERT_STRSCAN_FAILURE("10I", STRSCAN_OPT_TONUM);

	/* Integer suffices for floats do not work */
	ASSERT_STRSCAN_FAILURE("1.0U", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("1.0L", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("1.0UL", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("1.0LU", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("1.0LL", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("1.0ULL", STRSCAN_OPT_TONUM);
	ASSERT_STRSCAN_FAILURE("1.0LLU", STRSCAN_OPT_TONUM);

	/* Bad digits: */
	ASSERT_STRSCAN_FAILURE("012345678", STRSCAN_OPT_C);
	ASSERT_STRSCAN_FAILURE("012345679", STRSCAN_OPT_C);
	ASSERT_STRSCAN_FAILURE("0xBADDigit", STRSCAN_OPT_TONUM);
}

static void test_long_input(void **state)
{
	UNUSED_STATE(state);

	/* Tuned to exceed STRSCAN_MAXDIG from the sources */
	const char *TEN_E800 =
		"100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

	ASSERT_STRSCAN_PINF(TEN_E800);
	/* 2 ^ 67 */
	ASSERT_STRSCAN_GOOD_CONV_TO_NUM("0x80000000000000000",
					147573952589676412928.0);
}

static void test_imaginary(void **state)
{
	UNUSED_STATE(state);

	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("10I", 10.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("-10I", -10.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("+10i", 10.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("-10i", -10.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("0xaI", 10.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("-0xaI", -10.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("+0xAi", 10.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("-0xAi", -10.0);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("4.2I", 4.2);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("-4.2I", -4.2);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("+4.2i", 4.2);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("-4.2i", -4.2);

	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("0xF.FI", 15.9375);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("+0xf.fi", 15.9375);
	ASSERT_STRSCAN_GOOD_CONV_TO_IMAG("-0xf.Fi", -15.9375);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_decimal),
		cmocka_unit_test(test_hexadecimal),
		cmocka_unit_test(test_octal),
		cmocka_unit_test(test_special_floats),
		cmocka_unit_test(test_suffices),
		cmocka_unit_test(test_errors),
		cmocka_unit_test(test_long_input),
		cmocka_unit_test(test_imaginary),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
