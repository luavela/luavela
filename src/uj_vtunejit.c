/*
 * Client for the Intel VTune JIT API.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "uj_mem.h"
#include "uj_dispatch.h"
#include "uj_proto.h"

#if LJ_HASJIT

#include "jit/lj_jit.h"

#ifdef VTUNEJIT

#include "jitprofiling.h"

#define TRACE_NAME_BUFFER_SIZE 256

struct vtune_entry {
	char trace_name[TRACE_NAME_BUFFER_SIZE];
	char trace_loc[FORMATTED_LOC_BUF_SIZE];
	unsigned int vtune_trace_id;
};

void uj_vtunejit_addtrace(const jit_State *J, GCtrace *T)
{
	const GCproto *pt;
	const BCIns *pc;
	struct vtune_entry *eo;
	unsigned int vtune_trace_id;
	iJIT_Method_Load info = {0}; /* NB! Always locate on stack */

	if (iJIT_IsProfilingActive() != iJIT_SAMPLING_ON)
		return;

	pt = T->startpt;
	pc = T->startpc;

	lua_assert(pc >= proto_bc(pt) && pc < proto_bc(pt) + pt->sizebc);

	vtune_trace_id = iJIT_GetNewMethodID();

	eo = uj_mem_alloc(J->L, sizeof(*eo));
	T->vtunejit_entry = eo;

	eo->vtune_trace_id = vtune_trace_id;
	sprintf(eo->trace_name, "TRACE %u%s.%zu", T->traceno, J2G(J)->vmsuffix,
		curgeneration(J));
	uj_proto_sprintloc(eo->trace_loc, pt, proto_bcpos(pt, pc));

	info.method_id = vtune_trace_id;
	info.method_load_address = (void *)T->mcode;
	info.method_size = (unsigned int)T->szmcode;
	info.method_name = eo->trace_name;
	info.source_file_name = eo->trace_loc;

	iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, &info);
}

void uj_vtunejit_updtrace(const jit_State *J, const GCtrace *T)
{
	struct vtune_entry *eo;
	iJIT_Method_Load info = {0}; /* NB! Always locate on stack */

	UNUSED(J);

	if (iJIT_IsProfilingActive() != iJIT_SAMPLING_ON)
		return;

	eo = (struct vtune_entry *)T->vtunejit_entry;

	if (eo == NULL)
		return;

	info.method_id = eo->vtune_trace_id;
	info.method_load_address = T->mcode;
	info.method_size = (unsigned int)T->szmcode;

	iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_UPDATE, &info);
}

void uj_vtunejit_deltrace(const jit_State *J, const GCtrace *T)
{
	struct vtune_entry *eo = (struct vtune_entry *)T->vtunejit_entry;

	if (eo == NULL)
		return;

	uj_mem_free(MEM_G(J2G(J)), eo, sizeof(*eo));
}

#endif /* VTUNEJIT */
#endif /* LJ_HASJIT */
