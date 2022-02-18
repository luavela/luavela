-- Validation data for testing compatibility of bytecode dumpers,
-- bc.lua/LuaJIT vs. dump_bc/uJIT. We do not care about what the code below
-- actually does, the main goal of this file is to generate as diverse bytecode
-- as possible.
-- Important notes:
-- * Tests for return opcodes (RET, RET0, RET1, RETM) are spread across the file
-- * BC for function headers cannot be tested because:
--  ** Some opcodes are not dumped by bc.lua: FUNCF, FUNCV
--  ** Some opcodes are not emitted by the frontend:
--     IFUNCF, JFUNCF, IFUNCV, JFUNCV, FUNCC, FUNCCW, FUNC
-- * In uJIT 0.1 we significantly reduced the number of binary ops:
--   ADD{NV,VN,VV} -> ADD, SUB{NV,VN,VV} -> SUB, MUL{NV,VN,VV} -> MUL,
--   DIV{NV,VN,VV} -> DIV, MOD{NV,VN,VV} -> MOD. These opcodes are not tested
--   because LuaJIT and uJIT BC dumpers will obviously behave differently.
-- * uJIT's dumper is more aggressive about stripping too long string
--   constants, upvalue names and anything else that goes to a "hint" which is
--   printed after ';' in the dumped line. Currently hint buffer is limited to
--   80 chars, so this test does not cover corner cases with too long hint data.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

function comparison_ops()
    local a = 42
    local b = -42.42

    -- conditions
    if a <  b then print("cond01") end -- ISGE
    if a <= b then print("cond02") end -- ISGT
    if a >  b then print("cond03") end -- ISGE
    if a >= b then print("cond04") end -- ISGE
    if a == b then print("cond05") end -- ISNEV
    if a ~= b then print("cond06") end -- ISEQV

    if not (a <  b) then print("cond07") end -- ISLT
    if not (a <= b) then print("cond08") end -- ISLE
    if not (a >  b) then print("cond09") end -- ISLT
    if not (a >= b) then print("cond10") end -- ISLE

    if a == true then print("cond11") end -- ISNEP
    if a ~= true then print("cond12") end -- ISEQP

    if a == false then print("cond13") end -- ISNEP
    if a ~= false then print("cond14") end -- ISEQP

    if a == nil then print("cond15") end -- ISNEP
    if a ~= nil then print("cond16") end -- ISEQP

    if a == 42 then print("cond17") end -- ISNEN
    if a ~= 42 then print("cond18") end -- ISEQN

    if a == 2E+52 then print("cond19") end -- ISNEN
    if a ~= 2E+52 then print("cond20") end -- ISEQN

    if a == 'string' then print("cond21") end -- ISNES
    if a ~= 'string' then print("cond22") end -- ISEQS

    -- RET0
end

function unary_test_and_copy_ops()
    local a = 42
    local b = -42.42

    if     a then print("cond23") end
    if not a then print("cond24") end

    -- TODO: test what this emits
    local c =       a and b
    local d = (not a) and b
    local e =       a  or b
    local f = (not a)  or b

    return a, b, c, d, e, f  -- RET
end

function unary_ops(...)
    local a = 42
    local b = a     -- MOV
    local c = not a -- NOT
    local d = -a    -- UNM
    local e = #a    -- LEN

    return ... -- RETM
end

function binary_ops(a, b)
    local x = a  ^ b      -- POW
    local y = a .. b .. x -- CAT
end

-- NB! Some ops from this group are volens nolens emitted in other tests,
-- but let them be here as well:)
function constant_ops()
    -- TODO: emit KCDATA

    local a = 42      -- KSHORT
    local b = -42.42  -- KNUM
    local c = 2E+52   -- KNUM
    local d = "2E+52" -- KSTR

    local p0 = nil   -- KPRI
    local p1 = false -- KPRI
    local p2 = true  -- KPRI

    local x, y -- KNIL
end

function table_ops()
    local x = {[x] = 'x'} -- TNEW
    local y = {'y'}       -- TDUP

    local A = a -- GGET
    a = A       -- GSET

    local z = 'z'
    A      = x[z]   -- TGETV
    A      = x['x'] -- TGETS
    A      = x[1]   -- TGETB
    x[z]   = A      -- TSETV
    x['x'] = A      -- TSETS
    x[1]   = A      -- TSETB

    return { comparison_ops() } -- TSETM + RET1
end

function upvalue_and_function_ops()
    local names  = {"Peter"   , "Paul"   , "Mary"}
    local grades = { Peter = 8,  Paul = 7,  Mary = 10}

    function sortbygrade(names, grades)
        table.sort(names, function (n1, n2)
            local _grades = grades
            grades        = 'xxxGRADESxxx' -- USETS
            grades        = 2e52           -- USETN
            grades        = nil            -- USETP
            grades        = false          -- USETP
            grades        = true           -- USETP
            grades        = _grades        -- USETV
            return _grades[n1] > _grades[n2]
        end)
    end
end

function calls_and_vararg_handling(...)
    print(...)         -- VARG + CALLM
    print(42)          -- CALL
    return print('42') -- CALLT
end
function calls_and_vararg_handling_CALLMT(...)
    return print(42, ...) -- VARG + CALLMT
end

function loops_and_branches()
    -- NB! Some BCs are not emitted by the frontend:
    -- JFORI, IFORL, JFORL, IITERL, JITERL, ILOOP, JLOOP
    local x = {1, 2, 3}
    for _, _ in pairs(x) do    -- ISNEXT, ITERN, ITERL
        print('ok')
    end
    for _, _, _ in pairs(x) do -- ITERC, ITERL
        print('ok')
    end
    for _ = 100, 1, -1 do -- FORI, FORL
        print('ok')
    end
    while next(x) do -- LOOP
        print('ok')
    end
end

ujit.dump.bc(io.stdout, comparison_ops)
ujit.dump.bc(io.stdout, unary_test_and_copy_ops)
ujit.dump.bc(io.stdout, unary_ops)
ujit.dump.bc(io.stdout, binary_ops)
ujit.dump.bc(io.stdout, constant_ops)
ujit.dump.bc(io.stdout, table_ops)
ujit.dump.bc(io.stdout, upvalue_and_function_ops)
ujit.dump.bc(io.stdout, calls_and_vararg_handling)
ujit.dump.bc(io.stdout, calls_and_vararg_handling_CALLMT)
ujit.dump.bc(io.stdout, loops_and_branches)
