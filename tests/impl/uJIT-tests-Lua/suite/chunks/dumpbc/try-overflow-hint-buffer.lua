-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function bytecode_emitter()
    local var_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx = 'lol'

    local _ = 1.2345678901234567890123456789e17
    local _ = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    local _ = '\9\10\13\14\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127\127'

    local _ = function ()
        local _ = var_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
        var_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx = "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
    end
end

ujit.dump.bc(io.stdout, bytecode_emitter)
