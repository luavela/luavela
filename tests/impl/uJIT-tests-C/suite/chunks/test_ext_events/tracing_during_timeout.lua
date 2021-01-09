-- This is a part of uJIT's testing suite. The script is tuned to reproduce
-- following situation: During running the timeout function, the JIT compiler
-- starts recording a trace and aborts. The platform must not re-enable
-- timeout checks for the coroutine in these cirsumstances.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.on()

-- Ensures that JIT recorder will be started when trace_me is called 2nd time.
jit.opt.start("hotloop=1", "-jitcat")

timeout_handler_called = 0

local function trace_me()
        print("print") -- to abort tracing
        return "x"
end

function timeout_handler()
        timeout_handler_called = timeout_handler_called + 1

        local str = trace_me()
        local _ = ""

        -- This loop must will run uninterrupted because we are already in
        -- a timeout function.
        for i = 1, 1e5 do
                _ = str .. i -- ensures the loop is never compiled
        end
end

function coroutine_payload()
        local i = 0
        local s = 0
        local _
        while true do
                s = s + i
                i = i + 1
                _ = "str " .. i -- ensures the loop is never compiled
        end
end

trace_me()
