/*
 * Common definitions for the JIT compiler.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_JIT_H
#define _LJ_JIT_H

#include <stdio.h>

#include "lj_obj.h"
#include "jit/lj_ir.h"
#include "jit/lj_target.h"

/*
 * Layout of JIT engine flags (0 - free, 1 - used):
 * +MSB---------------------------------LSB+
 * |0011-1111-1111-1111-1111-0000-1111-0001|
 * +---------------------------------------+
 */

/* JIT engine flags. */
#define JIT_F_ON                0x00000001

/* CPU-specific JIT engine flags. */
#define JIT_F_CMOV              0x00000010
#define JIT_F_SSE2              0x00000020
#define JIT_F_SSE3              0x00000040
#define JIT_F_SSE4_1            0x00000080

/* Names for the CPU-specific flags. Must match the order above. */
#define JIT_F_CPU_FIRST         JIT_F_CMOV
#define JIT_F_CPUSTRING         "\4CMOV\4SSE2\4SSE3\6SSE4.1"

/* Optimization flags. */
#define JIT_F_OPT_MASK          0xfffff000

#define JIT_F_OPT_FOLD          0x00001000
#define JIT_F_OPT_CSE           0x00002000
#define JIT_F_OPT_DCE           0x00004000
#define JIT_F_OPT_FWD           0x00008000
#define JIT_F_OPT_DSE           0x00010000
#define JIT_F_OPT_NARROW        0x00020000
#define JIT_F_OPT_LOOP          0x00040000
#define JIT_F_OPT_ABC           0x00080000
#define JIT_F_OPT_SINK          0x00100000
#define JIT_F_OPT_FUSE          0x00200000
#define JIT_F_OPT_NOHREFK       0x00400000
#define JIT_F_OPT_NORETL        0x00800000
#define JIT_F_OPT_JITCAT        0x01000000
#define JIT_F_OPT_JITTABCAT     0x02000000
#define JIT_F_OPT_JITSTR        0x04000000
#define JIT_F_OPT_JITPAIRS      0x08000000
#define JIT_F_OPT_MOVTV         0x10000000
#define JIT_F_OPT_MOVTVPRI      0x20000000

/*
 * JIT_F_OPT_FUSE is a no-op under x86-64, and the flag is preserved for
 * compatibility only.
 */

/* Optimizations names for -O. Must match the order above. */
#define JIT_F_OPT_FIRST         JIT_F_OPT_FOLD

/* \nnn escape sequences are OCTAL: */
#define JIT_F_OPTSTRING \
  "\4fold\3cse\3dce\3fwd\3dse\6narrow\4loop\3abc\4sink\4fuse" \
  "\7nohrefk\6noretl\6jitcat\11jittabcat\6jitstr\10jitpairs\5movtv\10movtvpri"

/* Optimization levels set a fixed combination of flags. */
#define JIT_F_OPT_0     0
#define JIT_F_OPT_1     (JIT_F_OPT_FOLD| \
                          JIT_F_OPT_CSE       | \
                          JIT_F_OPT_DCE)
#define JIT_F_OPT_2     (JIT_F_OPT_1| \
                          JIT_F_OPT_NARROW    | \
                          JIT_F_OPT_LOOP)
#define JIT_F_OPT_3     (JIT_F_OPT_2| \
                          JIT_F_OPT_FWD       | \
                          JIT_F_OPT_DSE       | \
                          JIT_F_OPT_ABC       | \
                          JIT_F_OPT_SINK)
/* uJIT-specific optimizations: */
#define JIT_F_OPT_4     (JIT_F_OPT_3| \
                          JIT_F_OPT_FUSE      | \
                          JIT_F_OPT_NOHREFK   | \
                          JIT_F_OPT_NORETL    | \
                          JIT_F_OPT_JITCAT    | \
                          JIT_F_OPT_JITTABCAT | \
                          JIT_F_OPT_JITSTR)

#define JIT_F_OPT_DEFAULT  JIT_F_OPT_3

/* See: http://blogs.msdn.com/oldnewthing/archive/2003/10/08/55239.aspx */
#define JIT_P_sizemcode_DEFAULT         64

/* Optimization parameters and their defaults. Length is a char in octal! */
#define JIT_PARAMDEF(_) \
  _(\010, maxtrace,     1000)   /* Max. # of traces in cache. */ \
  _(\011, maxrecord,    4000)   /* Max. # of recorded IR instructions. */ \
  _(\012, maxirconst,   500)    /* Max. # of IR constants of a trace. */ \
  _(\007, maxside,      100)    /* Max. # of side traces of a root trace. */ \
  _(\007, maxsnap,      500)    /* Max. # of snapshots for a trace. */ \
  \
  _(\007, hotloop,      56)     /* # of iter. to detect a hot loop/call. */ \
  _(\007, hotexit,      10)     /* # of taken exits to start a side trace. */ \
  _(\007, tryside,      4)      /* # of attempts to compile a side trace. */ \
  \
  _(\012, instunroll,   4)      /* Max. unroll for instable loops. */ \
  _(\012, loopunroll,   15)     /* Max. unroll for loop ops in side traces. */ \
  _(\012, callunroll,   3)      /* Max. unroll for recursive calls. */ \
  _(\011, recunroll,    2)      /* Min. unroll for true recursion. */ \
  \
  /* Size of each machine code area (in KBytes). */ \
  _(\011, sizemcode,    JIT_P_sizemcode_DEFAULT) \
  /* Max. total size of all machine code areas (in KBytes). */ \
  _(\010, maxmcode,     8192) \
  /* End of list. */

enum {
#define JIT_PARAMENUM(len, name, value) JIT_P_##name,
JIT_PARAMDEF(JIT_PARAMENUM)
#undef JIT_PARAMENUM
  JIT_P__MAX
};

#define JIT_PARAMSTR(len, name, value)  #len #name
#define JIT_P_STRING    JIT_PARAMDEF(JIT_PARAMSTR)

/* Trace compiler state. */
typedef enum {
  LJ_TRACE_IDLE,        /* Trace compiler idle. */
  LJ_TRACE_ACTIVE = 0x10,
  LJ_TRACE_RECORD,      /* Bytecode recording active. */
  LJ_TRACE_START,       /* New trace started. */
  LJ_TRACE_END,         /* End of trace. */
  LJ_TRACE_ASM,         /* Assemble trace. */
  LJ_TRACE_ERR          /* Trace aborted with error. */
} TraceState;

/* Post-processing action. */
typedef enum {
  LJ_POST_NONE,         /* No action. */
  LJ_POST_FIXCOMP,      /* Fixup comparison and emit pending guard. */
  LJ_POST_FIXGUARD,     /* Fixup and emit pending guard. */
  LJ_POST_FIXGUARDSNAP, /* Fixup and emit pending guard and snapshot. */
  LJ_POST_FIXBOOL,      /* Fixup boolean result. */
  LJ_POST_FIXCONST,     /* Fixup constant results. */
  LJ_POST_FFRETRY,      /* Suppress recording of retried fast functions. */
  LJ_POST_FFSPECRET     /* Specialize TValue's returned by a fast function. */
} PostProc;

/* Machine code type. */
typedef uint8_t MCode;

/* Stack snapshot header. */
typedef struct SnapShot {
  uint32_t mapofs;      /* Offset into snapshot map. */
  IRRef1 ref;           /* First IR ref for this snapshot. */
  uint8_t nslots;       /* Number of valid slots. */
  uint8_t topslot;      /* Maximum frame extent. */
  uint8_t nent;         /* Number of compressed entries. */
  uint8_t count;        /* Count of taken exits for this snapshot. */
} SnapShot;

#define SNAPCOUNT_DONE  255     /* Already compiled and linked a side trace. */

/* Compressed snapshot entry. */
typedef uint32_t SnapEntry;

#define SNAP_FRAME      0x010000        /* Frame slot. */
#define SNAP_CONT       0x020000        /* Continuation slot. */
#define SNAP_LOW        0x040000        /* Restore only low 32 bits of slot. */
#define SNAP_NORESTORE  0x080000        /* No need to restore slot. */
#define SNAP_TREF_FLAGMASK (SNAP_FRAME | SNAP_CONT | SNAP_LOW)
LJ_STATIC_ASSERT(SNAP_FRAME == TREF_FRAME);
LJ_STATIC_ASSERT(SNAP_CONT == TREF_CONT);
LJ_STATIC_ASSERT(SNAP_LOW == TREF_LOW);
LJ_STATIC_ASSERT(SNAP_TREF_FLAGMASK == TREF_FLAGMASK);

#define SNAP(slot, flags, ref)  (((SnapEntry)(slot) << 24) + (flags) + (ref))
#define SNAP_TR(slot, tr) \
  (((SnapEntry)(slot) << 24) + ((tr) & (TREF_FLAGMASK | TREF_REFMASK)))
#define snap_ref(sn)            ((sn) & 0xffff)
#define snap_slot(sn)           ((BCReg)((sn) >> 24))
#define snap_isframe(sn)        ((sn) & SNAP_FRAME)
#define snap_setref(sn, ref)    (((sn) & (0xffff0000&~SNAP_NORESTORE)) | (ref))

/* Add PC to the snapshot map. Note that it occupies two entries. */
#define snap_store_pc(snapentry_p, pc) (*(const BCIns **)(void *)(snapentry_p) = (pc))
#define snap_pc(snapentry_p) (*(BCIns **)(void *)(snapentry_p))

/* Add ftsz to the snapshot map. Note that it occupies two entries. */
#define snap_store_ftsz(snapentry_p, ftsz) (*(uint64_t *)(void *)(snapentry_p) = (ftsz))

/* Snapshot and exit numbers. */
typedef uint32_t SnapNo;
typedef uint32_t ExitNo;

/* Trace number. */
typedef uint32_t TraceNo;       /* Used to pass around trace numbers. */
typedef uint16_t TraceNo1;      /* Stored trace number. */

/* Type of link. ORDER LJ_TRLINK */
typedef enum {
  LJ_TRLINK_NONE,               /* Incomplete trace. No link, yet. */
  LJ_TRLINK_ROOT,               /* Link to other root trace. */
  LJ_TRLINK_LOOP,               /* Loop to same trace. */
  LJ_TRLINK_TAILREC,            /* Tail-recursion. */
  LJ_TRLINK_UPREC,              /* Up-recursion. */
  LJ_TRLINK_DOWNREC,            /* Down-recursion. */
  LJ_TRLINK_INTERP,             /* Fallback to interpreter. */
  LJ_TRLINK_RETURN              /* Return to interpreter. */
} TraceLink;

/* Trace object. */
typedef struct GCtrace {
  GCHeader;
  uint16_t nsnap;       /* Number of snapshots. */
  IRRef nins;           /* Next IR instruction. Biased with REF_BIAS. */
  uint32_t padding;
  IRIns *ir;            /* IR instructions/constants. Biased with REF_BIAS. */
  GCobj *gclist;
  IRRef nk;             /* Lowest IR constant. Biased with REF_BIAS. */
  uint32_t nsnapmap;    /* Number of snapshot map elements. */
  SnapShot *snap;       /* Snapshot array. */
  SnapEntry *snapmap;   /* Snapshot map. */
  GCproto *startpt;     /* Starting prototype. */
  BCIns *startpc;       /* Bytecode PC of starting instruction. */
  BCIns startins;       /* Original bytecode of starting instruction. */
  size_t szmcode;       /* Size of machine code. */
  MCode *mcode;         /* Start of machine code. */
  size_t mcloop;                /* Offset of loop start in machine code. */
  uint16_t nchild;      /* Number of child traces (root trace only). */
  uint16_t spadjust;    /* Stack pointer adjustment (offset in bytes). */
  TraceNo1 traceno;     /* Trace number. */
  TraceNo1 link;        /* Linked trace (or self for loops). */
  TraceNo1 root;        /* Root trace of side trace (or 0 for root traces). */
  TraceNo1 nextroot;    /* Next root trace for same prototype. */
  TraceNo1 nextside;    /* Next side trace of same root trace. */
  uint8_t sinktags;     /* Trace has SINK tags. */
  uint8_t topslot;      /* Top stack slot already checked to be allocated. */
  uint8_t linktype;     /* Type of link. */
#ifdef UJIT_PROFILER
  uint8_t profcount;
#else
  uint8_t unused1;
#endif
#ifdef GDBJIT
  void *gdbjit_entry;   /* GDB JIT entry. */
#endif
#ifdef VTUNEJIT
  void *vtunejit_entry; /* VTune JIT entry. */
#endif
} GCtrace;

#define gco2trace(o)    lua_check((o)->gch.gct == ~LJ_TTRACE, (GCtrace *)(o))
#define traceref(J, n) \
  lua_check((n)>0 && (n)<(J)->sizetrace, (J)->trace[(n)])

LJ_STATIC_ASSERT(offsetof(GChead, gclist) == offsetof(GCtrace, gclist));

static LJ_AINLINE size_t snap_nextofs(GCtrace *T, SnapShot *snap)
{
  if (snap+1 == &T->snap[T->nsnap])
    return T->nsnapmap;
  else
    return (snap+1)->mapofs;
}

/* Round-robin penalty cache for bytecodes leading to aborted traces. */
typedef struct HotPenalty {
  BCIns *pc;            /* Starting bytecode PC. */
  uint16_t val;         /* Penalty value, i.e. hotcount start. */
  uint16_t reason;      /* Abort reason (really TraceErr). */
} HotPenalty;

#define PENALTY_SLOTS   64      /* Penalty cache slot. Must be a power of 2. */
#define PENALTY_MIN     (36*2)  /* Minimum penalty value. */
#define PENALTY_MAX     60000   /* Maximum penalty value. */
#define PENALTY_RNDBITS 4       /* # of random bits to add to penalty value. */

/* Round-robin backpropagation cache for narrowing conversions. */
typedef struct BPropEntry {
  IRRef1 key;           /* Key: original reference. */
  IRRef1 val;           /* Value: reference after conversion. */
  IRRef mode;           /* Mode for this entry (currently IRCONV_*). */
} BPropEntry;

/* Number of slots for the backpropagation cache. Must be a power of 2. */
#define BPROP_SLOTS     16

/* Scalar evolution analysis cache. */
typedef struct ScEvEntry {
  const BCIns *pc;      /* Bytecode PC of FORI. */
  IRRef1 idx;           /* Index reference. */
  IRRef1 start;         /* Constant start reference. */
  IRRef1 stop;          /* Constant stop reference. */
  IRRef1 step;          /* Constant step reference. */
  IRType1 t;            /* Scalar type. */
  uint8_t dir;          /* Direction. 1: +, 0: -. */
} ScEvEntry;

/* 128 bit SIMD constants. */
enum {
  LJ_KSIMD_ABS,
  LJ_KSIMD_NEG,
  LJ_KSIMD__MAX
};

/* Get 16 byte aligned pointer to SIMD constant. */
#define LJ_KSIMD(J, n) \
  ((TValue *)(((intptr_t)&(J)->ksimd[(n)] + 15) & ~(intptr_t)15))

/* Fold state is used to fold instructions on-the-fly. */
typedef struct FoldState {
  IRIns ins;            /* Currently emitted instruction. */
  IRIns left;           /* Instruction referenced by left operand. */
  IRIns right;          /* Instruction referenced by right operand. */
} FoldState;

/* Abort context for a given Lua state. */
typedef struct AbortState {
  lua_State   *L;  /* State that triggered abort. */
  const BCIns *pc; /* J->pc at the time of abort. */
  TValue       extra_data; /* For more verbose error reporting, filled on
                           ** synchronous abort inside JIT's event loop. */
} AbortState;

typedef struct K64Array K64Array;

/* JIT compiler state. */
typedef struct jit_State {
  GCtrace cur;          /* Current trace. */

  lua_State *L;         /* Current Lua state. */
  const BCIns *pc;      /* Current PC. */
  GCfunc *fn;           /* Current function. */
  GCproto *pt;          /* Current prototype. */
  TRef *base;           /* Current frame base, points into J->slots. */

  uint32_t flags;       /* JIT engine flags. */
  BCReg maxslot;        /* Relative to baseslot. */
  BCReg baseslot;       /* Current frame base, offset into J->slots. */

  uint8_t mergesnap;    /* Allowed to merge with next snapshot. */
  uint8_t needsnap;     /* Need snapshot before recording next bytecode. */
  IRType1 guardemit;    /* Accumulated IRT_GUARD for emitted instructions. */
  uint8_t bcskip;       /* Number of bytecode instructions to skip. */

  FoldState fold;       /* Fold state. */

  const BCIns *bc_min;  /* Start of allowed bytecode range for root trace. */
  size_t bc_extent;     /* Extent of the range. */

  TraceState state;     /* Trace compiler state. */

  int32_t instunroll;   /* Unroll counter for instable loops. */
  int32_t loopunroll;   /* Unroll counter for loop ops in side traces. */
  int32_t tailcalled;   /* Number of successive tailcalls. */
  int32_t framedepth;   /* Current frame depth. */
  int32_t retdepth;     /* Return frame depth (count of RETF). */

  K64Array *k64;                /* Pointer to chained array of 64 bit constants. */
  TValue ksimd[LJ_KSIMD__MAX+1];  /* 16 byte aligned SIMD constants. */

  IRIns *irbuf;         /* Temp. IR instruction buffer. Biased with REF_BIAS. */
  IRRef irtoplim;       /* Upper limit of instuction buffer (biased). */
  IRRef irbotlim;       /* Lower limit of instuction buffer (biased). */
  IRRef loopref;        /* Last loop reference or ref of final LOOP (or 0). */

  size_t sizesnap;      /* Size of temp. snapshot buffer. */
  SnapShot *snapbuf;    /* Temp. snapshot buffer. */
  SnapEntry *snapmapbuf;  /* Temp. snapshot map buffer. */
  size_t sizesnapmap;   /* Size of temp. snapshot map buffer. */

  PostProc postproc;    /* Required post-processing after execution. */

  GCtrace **trace;      /* Array of traces. */
  TraceNo freetrace;    /* Start of scan for next free trace. */
  size_t sizetrace;     /* Size of trace array. */

  IRRef1 chain[IR__MAX];  /* IR instruction skip-list chain anchors. */
  TRef slot[LJ_MAX_JSLOTS+LJ_STACK_EXTRA];  /* Stack slot map. */

  int32_t param[JIT_P__MAX];  /* JIT engine parameters. */

  MCode *exitstubgroup[LJ_MAX_EXITSTUBGR];  /* Exit stub group addresses. */

  HotPenalty penalty[PENALTY_SLOTS];  /* Penalty slots. */
  uint32_t penaltyslot; /* Round-robin index into penalty slots. */
  uint32_t prngstate;   /* PRNG state. */

  BPropEntry bpropcache[BPROP_SLOTS];  /* Backpropagation cache slots. */
  uint32_t bpropslot;   /* Round-robin index into bpropcache slots. */

  ScEvEntry scev;       /* Scalar evolution analysis cache slots. */

  const BCIns *startpc; /* Bytecode PC of starting instruction. */
  TraceNo parent;       /* Parent of current side trace (0 for root traces). */
  ExitNo exitno;        /* Exit number in parent of current side trace. */

  BCIns *patchpc;       /* PC for pending re-patch. */
  BCIns patchins;       /* Instruction for pending re-patch. */

  int mcprot;           /* Protection of current mcode area. */
  MCode *mcarea;        /* Base of current mcode area. */
  MCode *mctop;         /* Top of current mcode area. */
  MCode *mcbot;         /* Bottom of current mcode area. */
  size_t szmcarea;      /* Size of current mcode area. */
  size_t szallmcarea;   /* Total size of all allocated mcode areas. */

  size_t nsnaprestore;  /* Overall number of snap restores for all traces
                        ** "belonging" to the given jit_State
                        ** since the last call to luaE_metrics(). */
  size_t nflushall;     /* Number of successfull global flushes for the state. */
  FILE *dump_file;      /* if non-NULL: descriptor for dumping compiler's progress */

  AbortState abortstate; /* Substate filled on each trace abort. */
}
jit_State;

/* Assembler state. */
typedef struct ASMState {
  RegCost cost[RID_MAX];  /* Reference and blended allocation cost for regs. */

  MCode *mcp;           /* Current MCode pointer (grows down). */
  MCode *mclim;         /* Lower limit for MCode memory + red zone. */
#ifndef NDEBUG
  MCode *mcp_prev;      /* Red zone overflow check. */
#endif

  IRIns *ir;            /* Copy of pointer to IR instructions/constants. */
  jit_State *J;         /* JIT compiler state. */

  x86ModRM mrm;         /* Fused x86 address operand. */

  RegSet freeset;       /* Set of free registers. */
  RegSet modset;        /* Set of registers modified inside the loop. */
  RegSet weakset;       /* Set of weakly referenced registers. */
  RegSet phiset;        /* Set of PHI registers. */

  uint32_t flags;       /* Copy of JIT compiler flags. */
  int loopinv;          /* Loop branch inversion (0:no, 1:yes, 2:yes+CC_P). */

  int32_t evenspill;    /* Next even spill slot. */
  int32_t oddspill;     /* Next odd spill slot (or 0). */

  IRRef curins;         /* Reference of current instruction. */
  IRRef stopins;        /* Stop assembly before hitting this instruction. */
  IRRef orignins;       /* Original T->nins. */

  IRRef snapref;        /* Current snapshot is active after this reference. */
  IRRef snaprename;     /* Rename highwater mark for snapshot check. */
  SnapNo snapno;        /* Current snapshot number. */
  SnapNo loopsnapno;    /* Loop snapshot number. */

  IRRef sectref;        /* Section base reference (loopref or 0). */
  IRRef loopref;        /* Reference of LOOP instruction (or 0). */

  BCReg topslot;        /* Number of slots for stack check (unless 0). */
  int32_t gcsteps;      /* Accumulated number of GC steps (per section). */

  GCtrace *T;           /* Trace to assemble. */
  GCtrace *parent;      /* Parent trace (or NULL). */

  MCode *mcbot;         /* Bottom of reserved MCode. */
  MCode *mctop;         /* Top of generated MCode. */
  MCode *mcloop;        /* Pointer to loop MCode (or NULL). */
  MCode *invmcp;        /* Points to invertible loop branch (or NULL). */
  MCode *flagmcp;       /* Pending opportunity to merge flag setting ins. */
  MCode *realign;       /* Realign loop if not NULL. */

  IRRef1 phireg[RID_MAX];  /* PHI register references. */
  uint16_t parentmap[LJ_MAX_JSLOTS];  /* Parent instruction to RegSP map. */
} ASMState;

LJ_STATIC_ASSERT(sizeof(((jit_State *)0)->flags) == sizeof(((ASMState *)0)->flags));

/* Returns generation of the JIT state (interval # between two global flushes). */
#define curgeneration(J) ((J)->nflushall + 1)

/*
 * NB! Note on irt_* checks. Everything that depends on both trace and IR
 * should reside here, all the rest goes to lj_ir.h.
 */

static LJ_AINLINE int irt_ktdup(const GCtrace *T, const IRIns *ir)
{
  return ir->o == IR_TDUP && irt_kgc(&T->ir[ir->op1]);
}

/* Trivial PRNG e.g. used for penalty randomization. */
static LJ_AINLINE uint32_t LJ_PRNG_BITS(jit_State *J, int bits)
{
  /* Yes, this LCG is very weak, but that doesn't matter for our use case. */
  J->prngstate = J->prngstate * 1103515245 + 12345;
  return J->prngstate >> (32-bits);
}

#endif
