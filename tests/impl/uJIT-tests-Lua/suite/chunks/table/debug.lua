-- Tests for table-related introspection APIs. These are just basic
-- sanity tests, intentionally made scarse to avoid fragility.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local info

info = ujit.debug.gettableinfo({})
assert(info.asize == 0, "asize")
assert(info.hsize == 0, "hsize")
assert(info.acapacity == 0, "acapacity")
assert(info.hcapacity == 0, "hcapacity")
assert(info.hnchains == 0, "hnchains")
assert(info.hmaxchain == 0, "hmaxchain")

info = ujit.debug.gettableinfo({1, x = "y"})
assert(info.asize == 1, "asize")
assert(info.hsize == 1, "hsize")
assert(info.hnchains == 1, "hnchains")
assert(info.hmaxchain == 1, "hmaxchain")

info = ujit.debug.gettableinfo({1})
assert(info.asize == 1, "asize")
assert(info.hsize == 0, "hsize")
assert(info.hnchains == 0, "hnchains")
assert(info.hmaxchain == 0, "hmaxchain")

info = ujit.debug.gettableinfo({x = "y"})
assert(info.asize == 0, "asize")
assert(info.hsize == 1, "hsize")
assert(info.hnchains == 1, "hnchains")
assert(info.hmaxchain == 1, "hmaxchain")
