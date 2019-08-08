
# Usage

---

## Files

The directory `test_lua` contains the following files:

- *.t : test files written in Lua
- tap.lua : a minimal TAP library used when [Test.More](https://fperrad.frama.io/lua-TestMore/) is not available
- profile*.lua : a set of predefined profile files, `profile.lua` is loaded by default.
- Makefile : an helper

## Running the whole test suite

As tests produce a [TAP](http://en.wikipedia.org/wiki/Test_Anything_Protocol) output,
a TAP consumer like [prove](https://perldoc.perl.org/prove.html)
is required (`prove` comes with any Perl distribution, usually on GNU/Linux,
`prove` is already installed).

```
$ cd test_lua

$ prove --exec "SOMEWHERE/bin/lua -l profile_lua53" *.t

$ prove --exec "SOMEWHERE/bin/lua -l profile_lua53_noconv" *.t

$ prove --exec "SOMEWHERE/bin/luajit -l profile_luajit20" *.t

$ prove --exec "SOMEWHERE/bin/luajit -l profile_luajit21_compat52" *.t
```

Note: some tests using `luac` expect that the interpreter is named `lua`,
there fail when using something like `lua5.1`.

## Running a test

Without `prove`, the test launched by `lua` gives the TAP output.

```
$ cd test_lua
$ lua 090-tap.t
1..3
ok 1 - ok
ok 2 - 42 == 42
ok 3 - pass
```

## profile.lua

The features included (ie. compiled) in a Lua interpreter could be selected via this file

``` lua
local profile = {

--[[ compat 5.0
    has_string_gfind = true,
    has_math_mod = true,
--]]

    compat51 = true,
--[[
    has_unpack = true,
    has_package_loaders = true,
    has_math_log10 = true,
    has_loadstring = true,
    has_table_maxn = true,
    has_module = true,
    has_package_seeall = true,
--]]

    compat52 = true,
--[[
    has_mathx = true,
    has_bit32 = true,
    has_metamethod_ipairs = true,
--]]

    compat53 = false,
--[[
    has_math_log10 = true,
    has_mathx = true,
    has_metamethod_ipairs = true,
--]]

--[[ luajit
    luajit_compat52 = true,
--]]

}

package.loaded.profile = profile        -- prevents loading of default profile

return profile
```
