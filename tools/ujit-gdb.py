# GDB extension for uJIT post-mortem analysis.
# To use, put 'source tools/ujit-gdb.py' in gdb.
#
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

import re
import gdb

SIZE_OF_PTR = 8

FRAME_TYPE = 3
FRAME_P = 4
FRAME_TYPEP = FRAME_TYPE | FRAME_P

NO_REG = 0xff

BCMdst = 1
BCMbase = 2
BCMvar = 3
BCMrbase = 4
BCMnum = 9

BC_CALLM = 0x33
BC_CALL = 0x34
BC_CALLMT = 0x35
BC_CALLT = 0x36
BC_FUNCF = 0x4d
BC_IFUNCF = 0x4e
BC_JFUNCF = 0x4f
BC_FUNCV = 0x50
BC_IFUNCV = 0x51
BC_JFUNCV = 0x52
BC_FUNCC = 0x53
BC_FUNCCW = 0x54

FRAME_PCALL = 6

LJ_T__MIN = 0xfffffff2

FRAME_DEAD = 0
FRAME_ALIVE = 1

write = gdb.write

gtype_cache = {}


def gtype(typestr):
    global gtype_cache
    if typestr in gtype_cache:
        return gtype_cache[typestr]

    m = re.match(r"((?:(?:struct|union) )?\S*)\s*[*]", typestr)
    if m is None:
        gtype = gdb.lookup_type(typestr)
    else:
        gtype = gdb.lookup_type(m.group(1)).pointer()

    gtype_cache[typestr] = gtype
    return gtype


def cast(typestr, val):
    return gdb.Value(val).cast(gtype(typestr))


def sizeof(type):
    return gdb.lookup_type(type).sizeof


def tou64(gdbval):
    return int(gdbval.cast(gdb.lookup_type('long'))) & 0xFFFFFFFFFFFFFFFF


def i2notu64(val):
    return ~int(val) & 0xFFFFFFFFFFFFFFFF


def i2notu32(val):
    return ~int(val) & 0xFFFFFFFF


def tag_name(value_tag):
    return {
        i2notu32(0): "LJ_TNIL",
        i2notu32(1): "LJ_TFALSE",
        i2notu32(2): "LJ_TTRUE",
        i2notu32(3): "LJ_TLIGHTUD",
        i2notu32(4): "LJ_TSTR",
        i2notu32(5): "LJ_TUPVAL",
        i2notu32(6): "LJ_TTHREAD",
        i2notu32(7): "LJ_TPROTO",
        i2notu32(8): "LJ_TFUNC",
        i2notu32(9): "LJ_TTRACE",
        i2notu32(10): "LJ_TCDATA",
        i2notu32(11): "LJ_TTAB",
        i2notu32(12): "LJ_TUDATA",
        i2notu32(13): "LJ_TNUMX",
    }.get(value_tag, value_tag)


def vmstate_name(vmst):
    return {
        i2notu64(0): "LFUNC",
        i2notu64(1): "FFUNC",
        i2notu64(2): "CFUNC",
        i2notu64(3): "C",
        i2notu64(4): "INTERP",
        i2notu64(5): "GC",
        i2notu64(6): "EXIT",
        i2notu64(7): "RECORD",
        i2notu64(8): "OPT",
        i2notu64(9): "ASM",
    }.get(vmst, "LJ_TTRACE")


def frametype_name(ft):
    return {
        0: "L",
        1: "C",
        2: "M",
        3: "V",
    }.get(ft, "Unknown frame type")


def lj_bc_mode(op):
    return {
        BC_FUNCF: 0xb004,
        BC_IFUNCF: 0xb004,
        BC_JFUNCF: 0xb304,
        BC_FUNCV: 0xb004,
        BC_IFUNCV: 0xb004,
        BC_JFUNCV: 0xb304,
        BC_FUNCC: 0xb004,
        BC_FUNCCW: 0xb004,
        BC_CALLM: 0x4b32,
        BC_CALL: 0x4b32,
        BC_CALLMT: 0x4b02,
        BC_CALLT: 0x4b02,
    }.get(op, "not_found")


def isx2_bcm(m):
    return {
        BCMdst: 1,
        BCMvar: 1,
        BCMbase: 1,
        BCMrbase: 1,
        BCMnum: 1,
    }.get(m, 0)


def gc_states(state):
    return {
        0: "PAUSE",
        1: "PROPAGATE",
        2: "ATOMIC",
        3: "SWEEPSTRING",
        4: "SWEEP",
        5: "FINALIZE",
        6: "LAST",
    }.get(state, "UNKNOWN")


def bcmode_a(op):
    mode = lj_bc_mode(int(op))
    assert mode != 0
    return mode & 7


def isx2_a(o):
    return isx2_bcm(bcmode_a(o))


def bc_op(i):
    return i & 0xff


def x2_decode_r(i, r):
    ret = 0

    if isx2_a(bc_op(i)) and r != NO_REG:
        ret = r >> 1
    else:
        ret = r
    return ret


def bc_a_raw(ins):
    return (ins >> 8) & 0xff


def bc_a(ins):
    a = bc_a_raw(ins)
    return x2_decode_r(ins, a)


def vm_BASE():
    base = gdb.parse_and_eval("$r10")  # BASE register
    base = cast('union TValue *', base)
    return base


def vm_PC():
    pc = gdb.parse_and_eval('$rbx')
    pc = cast('BCIns *', pc)
    return pc


def frame_gc(framelink):
    func = framelink['fr']['func']
    func = cast('union GCobj *', func)
    return func


def frame_func(framelink):
    func = frame_gc(framelink)
    func = cast('union GCfunc *', func)
    return func


def frame_pc(framelink):
    pcr = framelink['fr']['tp']['pcr']
    return pcr[-1]


def frame_deltal(framelink):
    return 1 + bc_a(frame_pc(framelink))


def frame_prevl(framelink):
    return framelink - frame_deltal(framelink)


def frame_ftsz(framelink):
    return framelink['fr']['tp']['ftsz']


def frame_ispcall(framelink):
    return (frame_ftsz(framelink) & FRAME_PCALL) == FRAME_PCALL


def frame_sized(framelink):
    return (frame_ftsz(framelink) & ~FRAME_TYPEP) >> 4


def frame_prevd(framelink):
    return framelink - frame_sized(framelink)


def frame_type(framelink):
    return frame_ftsz(framelink) & FRAME_TYPE


def frame_typep(framelink):
    return frame_ftsz(framelink) & FRAME_TYPEP


def frame_islua(framelink):
    return frametype_name(int(frame_type(framelink))) == "L" \
        and int(frame_ftsz(framelink)) > 0


def frame_isc(framelink):
    return frametype_name(frame_type(framelink)) == "C"


def tagisvalid(slot):
    return slot['value_tag'] >= LJ_T__MIN


def get_gstate(L):
    if not isinstance(L, gdb.Value):
        L = cast('struct lua_State *', L)
    return cast('global_State *', L['glref'])


def vm_state(L):
    G = get_gstate(L)
    return vmstate_name(tou64(G['vmstate']))


def gc_state(L):
    G = get_gstate(L)
    state = G['gc']['state']
    return gc_states(int(state))


def pcr(framelink):
    return framelink['fr']['tp']['pcr']


def frame_prev(framelink):
    if frame_islua(framelink):
        return frame_prevl(framelink)
    else:
        return frame_prevd(framelink)


def funcproto(func):
    assert func['ffid'] == 0
    pc = func['pc']
    pc = cast('char *', pc)
    pc -= sizeof('struct GCproto')
    pt = cast('struct GCproto *', pc)
    return pt


def func_pc(func):
    return func['l']['pc']


def dump_gcfunc(func):
    func = cast('struct GCfuncC *', func)
    ffid = func['ffid']

    if ffid == 0:
        pt = funcproto(func)
        name = pt['chunkname']
        string = cast('struct GCstr *', name)
        string += 1  # String located at the end of GCstr.
        string = cast('char *', string)
        nupvals = func['nupvalues']
        # String will be printed with pointer to that string, thanks to gdb.
        write("Lua function {}, upvalues {}, {}:{}"
              .format(hex(tou64(func)), int(nupvals), string, pt['firstline']))
    elif ffid == 1:
        write("C function {}".format(hex(tou64(func['f']))))
    else:
        write("fast function {}".format(int(ffid)))


def strdata(obj):
    return str(cast('char *', cast('struct GCstr *', obj) + 1))


def dump_tvalue(addr):
    if not isinstance(addr, gdb.Value):
        addr = cast('union TValue *', addr)
    tag = tou64(addr['value_tag'])
    tgname = tag_name(tag)
    obj = addr['gcr']

    if tgname == "LJ_TSTR":
        write("string: {}".format(strdata(obj)))
    elif tgname == "LJ_TNUMX":
        fnum = cast('double', obj)
        write("number: {}".format(fnum))
    elif tgname == "LJ_TFUNC":
        dump_gcfunc(obj)
    elif tgname == "LJ_TNIL":
        write("nil")
    elif tgname == "LJ_TTRUE":
        write("true")
    elif tgname == "LJ_TFALSE":
        write("false")
    elif tgname == "LJ_TLIGHTUD":
        write("light user data {}".format(hex(tou64(obj))))
    elif tgname == "LJ_TCDATA":
        write("cdata {}".format(hex(tou64(obj))))
    elif tgname == "LJ_TUPVAL":
        write("upvalue {}".format(hex(tou64(obj))))
    elif tgname == "LJ_TTHREAD":
        write("thread {}".format(hex(tou64(obj))))
    elif tgname == "LJ_TTRACE":
        trace = cast('struct GCtrace *', obj)
        traceno = trace['traceno']
        write("trace {}".format(hex(tou64(traceno))))
    elif tgname == "LJ_TTAB":
        write("table {}".format(hex(tou64(obj))))
    elif tgname == "LJ_TUDATA":
        write("userdata {}".format(hex(tou64(obj))))
    else:
        write("not valid type {} at {}".format(tag, hex(tou64(addr))))


def print_framelink(L, fr, alive):
    stack = L['stack']
    ftype = frame_type(fr)
    fr_name = frametype_name(int(ftype))
    prev = frame_prev(fr)
    nslots = int((tou64(fr) - tou64(prev)) / 16)

    write("{}                [".format(fr))

    if fr == stack:
        write("S")
    else:
        write(" ")

    if pcr(fr) != 0:
        if alive == FRAME_ALIVE:
            write("   ] FRAME: [")
        else:
            write("   ] DEADF: [")

        if frame_ispcall(fr):
            write("PP")
        else:
            write(fr_name)
            if frame_typep(fr) & FRAME_P:
                write("P")
        write("] delta={}, ".format(nslots))
    else:
        write("   ] FRAME: dummy L\n")
        return

    dump_gcfunc(fr['fr']['func'])
    write("\n")


def print_stack_slot(L, slot, base=None, top=None):
    base = base or L['base']
    top = top or L['top']
    maxstack = L['maxstack']
    valid = tagisvalid(slot)

    if not valid:
        obj = slot['gcr']
        gct = i2notu32(obj['gch']['gct'])

        if tag_name(gct) == "LJ_TFUNC":
            print_framelink(L, slot, FRAME_DEAD)
            return

    write("{}                [ ".format(hex(tou64(slot))))

    if slot == base:
        write("B")
    else:
        write(" ")

    if slot == top:
        write("T")
    else:
        write(" ")

    if slot == maxstack:
        write("M")
    else:
        write(" ")

    if valid:
        write("] VALUE: ")
    else:
        write("] DEADF: ")

    dump_tvalue(slot)
    write("\n")


def dump_stack(L, base=None, top=None):
    if vm_state(L) != "LFUNC" and vm_state(L) != "CFUNC":
        print("[WARNING] Bad VM state: {}, must be CFUNC or LFUNC"
              .format(vm_state(L)))

    base = base or L['base']
    top = top or L['top']
    framelink = base - 1
    maxstack = L['maxstack']
    nfreeslots = int((tou64(maxstack) - tou64(top) - 16) / 16)

    print("{}:{} [    ] 5 slots: Red zone".format(hex(tou64(maxstack + 5)),
          hex((tou64(maxstack + 1)))))
    print("{}                [   M]".format(hex(tou64(maxstack))))
    print("{}:{} [    ] {} slots: Free stack slots".format(
          hex(tou64(maxstack - 1)), hex(tou64(top + 1)), nfreeslots))
    print("{}                [  T ]".format(hex(tou64(top))))

    slot = top - 1

    while pcr(framelink) != 0:
        while slot > framelink:
            print_stack_slot(L, slot, base, top)
            slot -= 1
        oldframe = framelink
        framelink = frame_prev(framelink)
        print_framelink(L, oldframe, FRAME_ALIVE)
        slot -= 1

    print_framelink(L, L['stack'], FRAME_ALIVE)


def parse_arg(arg):
    argv = gdb.string_to_argv(arg)

    if len(argv) == 0:
        raise gdb.GdbError("Wrong number of arguments. Use 'help <command>' "
                           "to get more info.")
    ret = gdb.parse_and_eval(arg)

    if not ret:
        raise gdb.GdbError("table argument empty")
    return ret


def call_c_func(func_name, args):
    arg_list = ','.join([str(arg) for arg in args])
    return gdb.parse_and_eval('{}({})'.format(func_name, arg_list))


def is_in_vm():
    frame_name = gdb.newest_frame().name()
    return frame_name.startswith('lj_BC_')


class UJDumpString(gdb.Command):
    """
uj-str address
Dumps the contents of the GCstr at address.
    """

    def __init__(self):
        super(UJDumpString, self).__init__(
            "uj-str", gdb.COMMAND_DATA
        )

    def invoke(self, arg, from_tty):
        addr = parse_arg(arg)
        write("string: {}\n".format(strdata(addr)))


UJDumpString()


class UJDumpTValue(gdb.Command):
    """
uj-tv L
Dumps the contents of the TValue at address.
    """

    def __init__(self):
        super(UJDumpTValue, self).__init__(
            "uj-tv", gdb.COMMAND_DATA
        )

    def invoke(self, arg, from_tty):
        addr = parse_arg(arg)
        addr = cast('union TValue *', addr)
        dump_tvalue(addr)
        write("\n")


UJDumpTValue()


class UJGlobalState(gdb.Command):
    """
uj-global-state L
Show current VM state.
    """

    def __init__(self):
        super(UJGlobalState, self).__init__(
            "uj-global-state", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        L = parse_arg(arg)
        L = cast('struct lua_State *', L)
        print("VM state: {}".format(vm_state(L)))
        print("GC state: {}".format(gc_state(L)))


UJGlobalState()


class UJDumpStack(gdb.Command):
    """
uj-stack L
Dumps Lua stack of the given coroutine L.
    """

    def __init__(self):
        super(UJDumpStack, self).__init__(
            "uj-stack", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        L = parse_arg(arg)
        L = cast('struct lua_State *', L)
        dump_stack(L)


UJDumpStack()


class UJDumpStackVM(gdb.Command):
    """
uj-stack-vm
Dumps Lua stack while VM executes bytecode.
    """

    def __init__(self):
        super(UJDumpStackVM, self).__init__("uj-stack-vm", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        if not is_in_vm():
            print("uj-stack-vm should only be called from VM (e.g. when"
                  " breaking on lj_BC_*)")
            return

        base = vm_BASE()
        framelink = base - 1

        func = frame_func(framelink)
        func = cast('struct GCfuncL *', func)

        pt = funcproto(func)
        top = base + pt['framesize']
        # unwind Lua VM stack until dummy L:
        while pcr(framelink) != 0:
            framelink = frame_prev(framelink)

        L_obj = frame_func(framelink)
        L = cast('struct lua_State *', L_obj)
        dump_stack(L, base, top)


UJDumpStackVM()


class UJDumpTable(gdb.Command):
    """
uj-tab address
Dumps the contents of the GCtab at address.
    """

    def __init__(self):
        super(UJDumpTable, self).__init__(
            "uj-tab", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        t = parse_arg(arg)
        t = cast('struct GCtab *', t)
        array = t['array']
        asize = t['asize']
        node = t['node']
        hsize = t['hmask'] + 1  # See hmask comment in lj_obj.h
        mt = t['metatable']

        if mt != 0:
            write("Metatable detected: {}\n".format(hex(tou64(mt))))

        print("Array part:")
        for i in range(int(asize)):
            write("[{}] ".format(i))
            dump_tvalue(array)
            write("\n")
            array += 1

        print("Hash part:")
        for i in range(int(hsize)):
            key = node['key']
            val = node['val']
            write("{ ")
            dump_tvalue(key)
            write(" } => { ")
            dump_tvalue(val)
            write(" } next = ")
            print("{}".format(node['next']))
            node += 1


UJDumpTable()


# Look up 'L' in the selected frame
def deduce_L():
    L = gdb.lookup_symbol('L')[0]
    if L:
        return L.value(gdb.selected_frame())


# Get L from arg if passed, otherwise hope that 'L' exists in current frame
def get_or_deduce_L(arg):
    L = parse_arg(arg) if arg else deduce_L()
    if not L:
        raise Exception("Can't deduce L (lua_State*) from current context")
    L = cast('struct lua_State *', L)
    return L


# Get function and pc of a previous frame
def get_prev_frame_func_and_pc(L):
    base = L['base']
    framelink = base - 1

    bc = pcr(framelink)
    bc = cast('BCIns *', bc)

    # To get function from previous frame, we need to go back a frame
    framelink = frame_prev(framelink)
    func = frame_func(framelink)
    return func, bc


# Get function and pc of a current frame, bc = currently executed bytecode
def get_func_and_bc_in_vm():
    framelink = vm_BASE() - 1
    func = frame_func(framelink)

    bc = vm_PC() - 1  # a bytecode on which breakpoint was set
    bc = cast('BCIns *', bc)

    return func, bc


# Returns:
#   * func - function which we want to dump
#   * bc - bytecode which will be highlighted in dump
def get_func_and_bc(arg):
    if is_in_vm():
        return get_func_and_bc_in_vm()
    else:
        L = get_or_deduce_L(arg)
        return get_prev_frame_func_and_pc(L)


class UJBC(gdb.Command):
    """
uj-bc [L]
Dumps bytecode of top Lua frame. Pass 'L' if it's not in local scope.
(no need to do it in VM)
    """

    def __init__(self):
        super(UJBC, self).__init__(
            "uj-bc", gdb.COMMAND_DATA
        )

    def invoke(self, arg, from_tty):
        if not is_in_vm():
            print("-- Currently in a C function. Dumping last Lua frame:")

        func, bc = get_func_and_bc(arg)
        hl_pos = bc - func_pc(func)
        call_c_func('uj_dump_bc_and_source', args=('stdout', func, hl_pos))


UJBC()


class UJTABCOLLISIONS(gdb.Command):
    """
uj-tab-collisions address
Dumps all collided keys followed by list of node indexes of the GCtab
at address. The last line contains number of colliding keys.
    """

    def __init__(self):
        super(UJTABCOLLISIONS, self).__init__(
            "uj-tab-collisions", gdb.COMMAND_DATA
        )

    def invoke(self, arg, from_tty):
        t = cast('struct GCtab *', parse_arg(arg))
        node = t['node']
        # See hmask comment in lj_obj.h
        hsize = t['hmask'] + 1
        mt = t['metatable']

        if mt != 0:
            write("Metatable detected: {}\n".format(hex(tou64(mt))))

        collides = {}
        for i in range(hsize):
            key = node[i]['key']
            tag = tag_name(tou64(key['value_tag']))
            if tag == 'LJ_TNIL':
                continue
            # Workaround for non LJ_TSTR type
            if tag != 'LJ_TSTR':
                write("Not a string key @ node[{}]\n".format(i))
                continue
            hk = strdata(key)
            collides[hk] = collides.setdefault(hk, []) + [i]

        count = 0
        for k in filter(lambda k: len(collides[k]) > 1, collides.keys()):
            count += 1
            write("{}: {}".format(k, collides[k]))

        write("Number of colliding keys: {}\n".format(count))


UJTABCOLLISIONS()


# GDB extension for fixing backtraces in uJIT's coredumps.
#
# The problem. As uJIT machine code is emitted dynamically,
# no debug information is available by default. To mitigate
# this issue, we compile uJIT with GDBJIT on. This
# results in DWARF data being emitted in run-time along with
# each compiled code trace. Emission is done in the manner
# compatible with gdb JIT interface (for details, see
# https://sourceware.org/gdb/onlinedocs/gdb/JIT-Interface.html).
# But this data is not loaded by gdb automatically when we work with
# a coredump instead of a live process. This script deals with
# this problem. For details, see LoadTraceDwarf docstring.
#

# We are running on x86_64 machines:
def read_ptr2uint(buf):
    return buf.cast(gtype("uint64_t *")).dereference()


def ptr2uint(val):
    UINT_T_FOR_PTR = "uint64_t"
    if re.match("0[xX][0-9a-fA-F]+", val):
        return cast(UINT_T_FOR_PTR, int(val, 16))
    return gdb.parse_and_eval(val).cast(gtype(UINT_T_FOR_PTR))


class LoadTraceDwarf(gdb.Command):
    """
load-trace-dwarf [address]

Traverses the list of in-memory DWARF data for compiled traces and
tries to find a trace which satisfies following condition:

    trace_pc <= address < trace_pc + trace_size_mcode

If address is omitted, %rip is used.

If the trace is found, its DWARF is appended to the symbol table.
The primary goal is to fix coredump's backtrace which is initially
partly/fully broken because of dynamic nature of JIT compilation.

Symbol __jit_debug_descriptor must be defined.
    """

    # .debug_info section's header position in the list of section headers,
    # hardcoded in uj_gdbjit.c
    GDBJIT_SECT_debug_info = 6

    # Honest parsing of .debug_info section should be done using hints
    # recorded in .debug_abbrev section. But we are cheating: Our goal is
    # to retrieve DW_AT_low_pc and DW_AT_high_pc which are located
    # [fixed number of bytes] + strlen(filename) relative to .debug_info's
    # start. See gdbjit_debuginfo (uj_gdbjit.c) for more details.
    DEBUG_INFO_OFFSET_INIT = 4 + 2 + 4 + 1 + 1

    def __init__(self):
        super(LoadTraceDwarf, self).__init__(
            "load-trace-dwarf", gdb.COMMAND_DATA
        )

    def invoke(self, arg, from_tty):
        argv = gdb.string_to_argv(arg)
        if len(argv) > 1:
            raise gdb.GdbError("load-trace-dwarf takes 0 or 1 argument.")

        target_pc = ptr2uint(argv[0] if len(argv) == 1 else "$rip")

        dbg_desc = gdb.lookup_global_symbol("__jit_debug_descriptor")
        if not dbg_desc:
            raise gdb.GdbError("__jit_debug_descriptor not found")

        print("Scanning entries in __jit_debug_descriptor")

        i = 0
        entry = dbg_desc.value()["first_entry"]
        try:
            while (entry != 0x0):
                i += 1
                if self.find_and_load_trace_dwarf(target_pc, entry, i):
                    return
                entry = entry["next_entry"]
        except gdb.MemoryError:
            print((
                "WARNING: "
                "Traversal of __jit_debug_descriptor aborted "
                "(memory access error)"
            ))

        print("{} entries scanned, nothing relevant found".format(str(i)))

    def find_and_load_trace_dwarf(self, target_pc, entry, idx):
        # NB! To fix the backtrace it is almost always enough to supply
        # the 0th frame (the trace) with DWARF metadata. However, it is
        # possible (at least in theory) that we do a callq to a C function
        # inside the trace, crash there and after that everything
        # above the initial callq frame gets broken. If we ever face this
        # isssue, we'll solve it separately.

        # For each compilation unit compiled with a DWARF producer,
        # a contribution is made to the .debug_info section of the object file.
        # The code below finds the beginning of section's contents.
        elf_raw = entry["symfile_addr"]
        elf_parsed = elf_raw.cast(gtype("struct gdb_jit_obj *"))
        sh_debug_info = elf_parsed["sect"][self.GDBJIT_SECT_debug_info]

        if sh_debug_info.type.name != "Elf64_Shdr":
            raise gdb.GdbError("INTERNAL: Unknown sh_debug_info.type.name")

        die_cu = elf_raw + sh_debug_info["sh_offset"]
        offset = self.DEBUG_INFO_OFFSET_INIT

        # DW_AT_name is represented as DW_FORM_string, variable-length sequence
        # of non-null bytes followed by a null byte (as per DWARF definition):
        while die_cu[offset] != 0:
            offset += 1
        offset += 1  # Terminating \0 of DW_AT_name

        low_pc = read_ptr2uint(die_cu + offset)
        high_pc = read_ptr2uint(die_cu + offset + SIZE_OF_PTR)

        if not (low_pc <= target_pc < high_pc):
            return False

        print("FOUND: {}".format(str(idx)))
        fname = "/tmp/trace_{}.sym".format(str(idx))
        dump_cmd = "dump binary memory {} {} {}".format(
            fname,
            str(elf_raw.cast(gtype("void *"))),                         # start
            str((elf_raw + entry["symfile_size"]).cast(gtype("void *")))  # end
        )
        add_cmd = "add-symbol-file {} {}".format(fname, str(low_pc))

        gdb.execute(dump_cmd)
        print("Dumped to {}".format(fname))
        gdb.execute(add_cmd, False, False)
        print("Try to run bt now")

        return True


LoadTraceDwarf()
