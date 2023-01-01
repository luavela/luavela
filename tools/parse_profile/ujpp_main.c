/*
 * uJIT profiler parser.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "ujpp_main.h"
#include "ujpp_utils.h"
#include "ujpp_parser.h"
#include "ujpp_callgraph.h"
#include "ujpp_output.h"
#include "ujpp_read.h"
#include "ujpp_state.h"
#include "../../src/uj_vmstate.h"
#include "../../src/profile/ujp_write.h"

#define EVENT_MASK 0x0F

static int main_read_event(struct parser_state *ps)
{
	uint8_t ev_header;
	uint8_t vmstate;
	struct reader *r = &ps->reader;

	ev_header = ujpp_read_u8(r);

	if (ev_header == UJP_EPILOGUE_BIT)
		return 1;

	vmstate = ev_header & EVENT_MASK;
	if (vmstate > UJ_VMST_TRACE)
		ujpp_utils_die("wrong vmstate", NULL);

	ps->_vmstates[vmstate]++;
	ps->total++;
	ujpp_parser_read_stack(ps, vmstate);

	return 0;
}

static void main_read_epilogue(struct parser_state *ps)
{
	uint64_t end_time;
	uint64_t num_samples;
	uint64_t num_overruns;
	uint64_t secs;
	uint64_t minutes;
	uint64_t last;
	double overruns_share;
	struct reader *r = &ps->reader;

	end_time = ujpp_read_u64(r), num_samples = ujpp_read_u64(r),
	num_overruns = ujpp_read_u64(r),
	secs = (end_time - ps->start_time) / 1000000, minutes = secs / 60,
	last = secs - minutes * 60;
	overruns_share = (double)num_overruns / (num_samples + num_overruns);

	printf("end_time       = ");
	ujpp_utils_print_date(end_time);

	printf("duration       = %" PRIu64 " minute(s), %" PRIu64 " second(s)\
	       \nnum_samples    = %" PRIu64 "\
	       \nnum_overruns   = %" PRIu64 "\
	       \noverruns_share = %0.4f\n",
	       minutes, last, num_samples, num_overruns, overruns_share);
}

static void main_read_prologue(struct parser_state *ps)
{
	uint64_t format_ver;
	uint64_t data_id;
	uint64_t interval;
	uint64_t so_num;
	char sign[3];
	struct reader *r = &ps->reader;

	sign[0] = ujpp_read_u8(r);
	sign[1] = ujpp_read_u8(r);
	sign[2] = ujpp_read_u8(r);

	if (sign[0] != 'u' || sign[1] != 'j' || sign[2] != 'p')
		ujpp_utils_die("bad file format", NULL);

	format_ver = ujpp_read_u64(r);

	if (format_ver != UJP_CURRENT_FORMAT_VERSION)
		ujpp_utils_die("wrong version %" PRIu64 ", current is %d",
			       format_ver, UJP_CURRENT_FORMAT_VERSION);

	/* 3 zeroes */
	ujpp_read_u64(r);
	ujpp_read_u64(r);
	ujpp_read_u64(r);

	data_id = ujpp_read_u64(r);
	ps->start_time = ujpp_read_u64(r);
	interval = ujpp_read_u64(r);
	so_num = ujpp_read_u64(r);

	printf("uJIT profile | UJP event stream v%" PRIu64 "\
	       \nprofile_id     = 0x%" PRIx64 "\
	       \ninterval       = %" PRIu64 " usec\
	       \nloaded_so_num  = %" PRIu64 "\
	       \nstart_time     = ",
	       format_ver, data_id, interval, so_num);
	ujpp_utils_print_date(ps->start_time);
	ujpp_utils_read_so(ps, so_num);
}

UJPP_STATIC_ASSERT(offsetof(struct lfunc, count) == 0);
UJPP_STATIC_ASSERT(offsetof(struct cfunc, count) == 0);
UJPP_STATIC_ASSERT(offsetof(struct ffunc, count) == 0);
static uint64_t main_sum_counts(struct vector *vec)
{
	uint64_t sum = 0;
	for (size_t i = 0; i < ujpp_vector_size(vec); ++i) {
		/*
		 * Don't care about actual type as long as
		 * assertions above are valid
		 */
		const struct cfunc *f = ujpp_vector_at(vec, i);

		sum += f->count;
	}
	return sum;
}

/*
 * FFUNCs that have no assembler definition are streamed as CFUNCs.
 * Hence need to update CFUNC, FFUNC (and LFUNC just in case) counters
 * when event stream parsing is done.
 */
static void main_update_func_counters(struct parser_state *ps)
{
	ps->_vmstates[UJ_VMST_LFUNC] = main_sum_counts(&ps->vec_lfunc);
	ps->_vmstates[UJ_VMST_CFUNC] = main_sum_counts(&ps->vec_cfunc);
	ps->_vmstates[UJ_VMST_FFUNC] = main_sum_counts(&ps->vec_ffunc);
}

static void main_parse_file(struct parser_state *ps)
{
	main_read_prologue(ps);
	while (main_read_event(ps) == 0)
		;
	main_read_epilogue(ps);
	main_update_func_counters(ps);
	ujpp_output_print(ps);
}

int main(int argc, char **argv)
{
	const char *short_opt = "hp:c:e:";
	struct parser_state state;
	struct parser_state *ps = &state;
	struct option long_opt[] = {{"help", no_argument, NULL, 'h'},
				    {"profile", required_argument, NULL, 'p'},
				    {"callgraph", required_argument, NULL, 'c'},
				    {"exec", required_argument, NULL, 'e'},
				    {NULL, 0, NULL, 0}};

	ujpp_state_init(ps, argc, argv, short_opt, long_opt);
	main_parse_file(ps);
	ujpp_callgraph_generate(ps);
	ujpp_state_free(ps);

	return 0;
}
