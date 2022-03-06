-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function bytecode_emitter()
    local _ = function()
        -- emits FNEW
    end
end

ujit.dump.bc(io.stdout, bytecode_emitter)

