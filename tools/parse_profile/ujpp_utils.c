/*
 * This module implements auxiliary functions for ffunc's demangling, qsort
 * comparators, etc.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "ujpp_utils.h"
#include "ujpp_main.h"
#include "ujpp_read.h"
#include "ujpp_demangle_lua.h"

#include "uj_vmstate.h"

const char *const lj_vmstate_names[] = {
#define VMSTATENAME(name) (#name),
	VMSTATEDEF(VMSTATENAME)
#undef VMSTATENAME
	/* sentinel */
	"TRACE"};

const char *ffunc_names[] = {
	"FF_C",
	"FF_LUA",
#define FFDEF(name) (#name ""),
#include "lj_ffdef.h"
};

const char *ujpp_utils_vmst_name(const size_t id)
{
	return lj_vmstate_names[id];
}

const char *ujpp_utils_ffunc_name(const size_t id)
{
	return ffunc_names[id];
}

const char *ujpp_utils_vmst_counter_name(struct parser_state *ps,
					 size_t counter)
{
	for (size_t i = 0; i < UJ_PROFILE_NUM_DISTINCT_VM_STATES; i++)
		if (counter == ps->vmstates_demangle[i]) {
			ps->vmstates_demangle[i] = (size_t)-1;
			return ujpp_utils_vmst_name(i);
		}

	ujpp_utils_die("can't demangle vmstate", NULL);
	return NULL;
}

void ujpp_utils_print_date(const uint64_t msecs)
{
	struct tm t;
	char buf[BUFSIZ] = {0};
	struct timespec ts;

	ts.tv_sec = msecs / 1000000;
	ts.tv_nsec = ((msecs / 1000) % 1000) * 1000000;

	tzset();
	if (!gmtime_r(&ts.tv_sec, &t)) {
		printf("%" PRIx64 "\n", msecs);
		return;
	}

	if (!strftime(buf, BUFSIZ, "%F %T", &t)) {
		printf("%" PRIx64 "\n", msecs);
		return;
	}
	printf("%s UTC\n", buf);
}

static enum so_type utils_obj_type(const char *path)
{
	int is_vdso = strcmp(path, VDSO_NAME) == 0;

	if (strstr(path, ".so") != NULL)
		return SO_SHARED;
	else if (is_vdso)
		return SO_VDSO;
	else
		return SO_BIN;
}

void ujpp_utils_read_so(struct parser_state *ps, uint64_t so_num)
{
	struct reader *r = &ps->reader;

	for (size_t i = 0; i < so_num; i++) {
		char *path = ujpp_read_str(r);
		uint64_t base = ujpp_read_u64(r);
		enum so_type type = utils_obj_type(path);

		if (type == SO_BIN && ps->exec_file_name != NULL) {
			/* Replace executable with file, defined by --exec. */
			free(path);
			path = ps->exec_file_name;
		}

		ujpp_demangle_load_so(&ps->vec_loaded_so, path, base, type);
	}
}

void ujpp_utils_die_nofunc(const char *fmt, ...)
{
	va_list argp;

	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
	fputc('\n', stderr);
	exit(1);
}

/* Compares number of calls of CFUNC, LFUNC, FFUNC, traces and C states */
static int utils_cmp_counters(const void *first, const void *second)
{
	const struct cfunc *elem1 = *(struct cfunc **)first;
	const struct cfunc *elem2 = *(struct cfunc **)second;

	return (int)(int64_t)(elem2->count - elem1->count);
}

/* Compares vmstates counters */
int ujpp_utils_cmp_vmstate(const void *first, const void *second)
{
	size_t elem1 = *(size_t *)first;
	size_t elem2 = *(size_t *)second;

	return (int)(elem2 - elem1);
}

/* Compares 2 stacks */
int ujpp_utils_cmp_stack(const void *first, const void *second)
{
	const struct stack *s1 = (struct stack *)first;
	const struct stack *s2 = (struct stack *)second;

	if (s1->v->size != s2->v->size)
		return 1;

	for (size_t i = 0; i < s1->v->size; i++) {
		struct frame *f1 = s1->v->elems[i];
		struct frame *f2 = s2->v->elems[i];

		if (f1->type != f2->type || f1->cache_id != f2->cache_id)
			return 1;
	}

	return 0;
}

/* Compares 2 traces */
int ujpp_utils_cmp_trace(const void *first, const void *second)
{
	const struct trace *t1 = (struct trace *)first;
	const struct trace *t2 = (struct trace *)second;
	int eq = t1->generation == t2->generation && t1->traceno == t2->traceno;

	if (eq && t1->name && t2->name &&
	    (0 != strcmp(t1->name, t2->name) || t1->line != t2->line))
		ujpp_utils_die("Trace symbol collision: two traces have the "
			       "same generation %d and trace number %d, but "
			       "different names or lines - %s:%d vs %s:%d",
			       t1->generation, t1->traceno, t1->name, t1->line,
			       t2->name, t2->line);

	return !eq;
}

void utils_prepare_vmstates(struct parser_state *ps)
{
	memcpy(ps->vmstates_sorted, ps->_vmstates, sizeof(ps->_vmstates));
	memcpy(ps->vmstates_demangle, ps->_vmstates, sizeof(ps->_vmstates));

	qsort(ps->vmstates_sorted, UJ_PROFILE_NUM_DISTINCT_VM_STATES,
	      sizeof(size_t), ujpp_utils_cmp_vmstate);
}

void ujpp_utils_sort_items(struct parser_state *ps)
{
	struct hvmstate_info *hvmstis = ps->hvmst_infos;

	utils_prepare_vmstates(ps);
	ujpp_hash_2_vector(&ps->ht_trace, &ps->vec_trace);
	ujpp_demangle(ps);
	ujpp_demangle_lua(&ps->vec_lfunc_cache);

	qsort(ps->vec_lfunc.elems, ps->vec_lfunc.size, sizeof(void *),
	      utils_cmp_counters);
	qsort(ps->vec_cfunc.elems, ps->vec_cfunc.size, sizeof(void *),
	      utils_cmp_counters);
	qsort(ps->vec_ffunc.elems, ps->vec_ffunc.size, sizeof(void *),
	      utils_cmp_counters);
	qsort(ps->vec_trace.elems, ps->vec_trace.size, sizeof(void *),
	      utils_cmp_counters);

	for (size_t hvmst = UJ_VMST_HVMST_START; hvmst < UJ_VMST__MAX;
	     hvmst++) {
		struct hvmstate_info *hvmsti = &hvmstis[hvmst];

		qsort(hvmsti->vec_rip.elems, hvmsti->vec_rip.size,
		      sizeof(void *), utils_cmp_counters);
	}
}

void *ujpp_utils_allocz(size_t sz)
{
	void *p = calloc(1, sz);

	if (p == NULL) {
		ujpp_utils_die("failed to allocate %zu bytes", NULL);
		return NULL; /* unreachable. */
	}

	return p;
}

char *ujpp_utils_map_file(const char *path, size_t *fsize)
{
	int fd;
	size_t sz;
	void *buf;
	struct stat st;

	fd = open(path, O_RDONLY);

	if (-1 == fd)
		return NULL;

	if (-1 == fstat(fd, &st))
		goto err_exit;

	sz = st.st_size;

	if (0 == sz)
		goto err_exit;

	buf = mmap(0, sz, PROT_READ, MAP_SHARED, fd, 0);

	if (MAP_FAILED == buf)
		goto err_exit;

	close(fd); /* Igore errors. */
	*fsize = sz;
	return (char *)buf;
err_exit:
	close(fd); /* Igore errors. */
	return NULL;
}

int ujpp_utils_unmap_file(void *mem, size_t sz)
{
	return munmap(mem, sz);
}
