-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local chunks_dir = arg[1] or '.'
local classify_checks = assert(dofile(chunks_dir .. '/fp-classify.lua'))
local coercion_checks = assert(dofile(chunks_dir .. '/coercion.lua'))

jit.on()
jit.opt.start(0, "hotloop=1", "hotexit=1")

-- We get a trace with all possible variations of generated IRs by disabling optimization
-- and running tests written in 'inf.lua' and 'coercion.lua'

for i = 1, 100 do
    local fnum = i * 1.2
    classify_checks(fnum)
    coercion_checks(tostring(fnum))
end

jit.off()
