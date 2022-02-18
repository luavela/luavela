/*
 * This module implements Lua functions name "demangling".
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <ctype.h>

#include "ujpp_main.h"
#include "ujpp_utils.h"
#include "ujpp_demangle_lua.h"

#define LUA_TRUNCATE_LEN 60

/* Returns pointer to the beginning of line with number n. */
static const char *demangle_lua_count_line(const char *buf, size_t len,
					   const size_t n)
{
	size_t line = 1;

	for (size_t i = 0; i < len; i++) {
		if (n == line)
			return &buf[i];
		if ('\n' != buf[i])
			continue;
		line++;
	}

	return DEMANGLE_LUA_FAILED;
}

/* Returned when Lua symbol can't be demangled. */
static const char *demangle_lua_anon_name(void)
{
	const char *anon = "anonymous";
	char *buf;

	buf = ujpp_utils_allocz(strlen(anon) + 1);
	memcpy(buf, anon, strlen(anon));

	return buf;
}

static int isspecsym(char c)
{
	return '(' == c || '_' == c;
}

static const char *demangle_lua_find_lbra(const char *buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if ('(' == buf[i])
			return &buf[i];
	}

	return NULL;
}

static int demangle_lua_isnamechar(char c)
{
	return isalpha(c) || isdigit(c) || '_' == c || '.' == c || ':' == c;
}

static const char *demangle_lua_parse_name(const char *str, size_t len)
{
	char *func = strstr(str, "function");
	const char *end;
	size_t name_len; /* Lua symbol length. */
	char *lua_name; /* Pointer to demangled symbol. */
	size_t pos = 0;

	if (NULL == func)
		return DEMANGLE_LUA_FAILED;

	func += strlen("function");

	/* Remove leading spaces, tabs, newlines, etc. */
	while (!isalpha(*func) && !isdigit(*func) && !isspecsym(*func))
		func++;

	/* func can point to '('. */
	end = demangle_lua_find_lbra(func, len);

	if (NULL == end)
		return demangle_lua_anon_name();

	name_len = end - func;

	/* String is filled up with spaces. */
	if (!name_len)
		return demangle_lua_anon_name();

	lua_name = ujpp_utils_allocz(name_len + 1);

	for (size_t i = 0; i < name_len; i++) {
		if (demangle_lua_isnamechar(func[i])) {
			lua_name[pos] = func[i];
			pos++;
		}
	}

	if (name_len > LUA_TRUNCATE_LEN)
		lua_name[LUA_TRUNCATE_LEN] = '\0';

	return lua_name;
}

/*
 * Returns demangled string. One function must be defined at one line.
 * Dont use any comments between 'function' and '('.
 */
static const char *demangle_lua(char *buf, size_t sz, size_t line)
{
	const char *func; /* Pointer to line with 'function' keyword. */

	func = demangle_lua_count_line(buf, sz, line);

	if (DEMANGLE_LUA_FAILED == func)
		return DEMANGLE_LUA_FAILED;

	return demangle_lua_parse_name(func, sz - (size_t)(func - buf));
}

/* Optimised to avoid re-opening already read files. */
void ujpp_demangle_lua(struct vector *lfunc_cache)
{
	size_t sz = ujpp_vector_size(lfunc_cache);

	for (size_t i = 0; i < sz; i++) {
		struct lfunc_cache *lfc = ujpp_vector_at(lfunc_cache, i);
		size_t fsz = 0;
		char *buf = NULL;

		if (lfc->demangled_sym)
			continue;

		/* NULL as return value is OK. */
		buf = ujpp_utils_map_file(lfc->sym + 1, &fsz);

		/*
		 * Iterates next entries and finds what symbols belong to
		 * the opened file.
		 */
		for (size_t j = i; j < sz; j++) {
			struct lfunc_cache *lfc_next =
				ujpp_vector_at(lfunc_cache, j);

			/* Already demangled. */
			if (NULL != lfc_next->demangled_sym)
				continue;

			/* Belongs to another Lua file. */
			if (0 != strcmp(lfc_next->sym, lfc->sym))
				continue;

			assert(NULL == lfc_next->demangled_sym);

			if (NULL == buf) {
				lfc_next->demangled_sym = DEMANGLE_LUA_FAILED;
				continue;
			}

			lfc_next->demangled_sym =
				demangle_lua(buf, fsz, lfc_next->line);
		}

		if (buf)
			ujpp_utils_unmap_file(buf, fsz);
	}
}
