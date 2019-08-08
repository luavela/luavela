-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- all_cases() is FUNCF ;)
function all_cases()
    -- FORL
    local a = 0
    for _ = 1, 10 do
        a = a + 1
    end
    print(a)

    -- LOOP goto
    local b = 0
    ::lable1::
    b = b + 1
    if (b > 10) then
        goto lable2
    end
    goto lable1
    ::lable2::
    print(b)

    -- LOOP while
    local c = 0
    while (c < 10) do
        c = c + 1
    end
    print(c)

    -- LOOP repeat
    local d = 0
    repeat
        d = d + 1
    until d > 10
    print(d)

    -- ITERL
    local e = { 5, 5, 5, 5, 5 }
    local sum = 0
    for _, v in ipairs(e) do
        sum = sum + v
    end
    print(sum)
end

function update_counters(cnt)
    jit.opt.start("hotloop=" .. cnt)
end

dumped_all_cases = string.dump(all_cases)
loaded_all_cases = loadstring(dumped_all_cases)
