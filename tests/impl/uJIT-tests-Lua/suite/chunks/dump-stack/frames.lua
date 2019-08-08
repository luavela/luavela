-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Dumping frames of various types
--

local function mt_index(t, key)
    if t.__nils[key] then
        return
    end

    local value = t.__defined[key]

    ujit.dump.stack(io.stdout)

    return value
end

local function create(defined, nils)
    return setmetatable({
        __defined = defined,
        __nils    = nils,
    }, {
        __index = mt_index,
    })
end

local function foo(bar)
    local o      = create({['ip'] = '127.0.0.1'}, {['foo'] = true})
    local result = 'result:' .. string.char(bar % 255) .. o.ip

    print(#result)
    return result
end

pcall(foo, 9)
