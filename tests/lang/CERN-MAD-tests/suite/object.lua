-- UJIT: Code in this chunk was taken from luaobject.lua because of uJIT's
-- UJIT: stricter requirements to the number of local variables per function.

-- UJIT: table.new and table.clear are introduced in LuaJIT 2.1, and
-- UJIT: are currently not a part of the Language Reference Manual.
-- UJIT: We provide stub implementation for the sake of suite completeness.
-- require 'table.clear'
-- require 'table.new'
table.new   = table.new or function(_, _) return {} end
table.clear = table.clear or function(t)
  for k, v in pairs(t) do
    t[k] = nil
  end
end
-- UJIT: Another question is the usage of the non-standard bit library,
-- UJIT: but it is left as is (for the sake of suite completeness, sigh).

-- operators
local bit = require 'bit'
local tobit = bit.tobit
local bnot, band, bor, rol = bit.bnot, bit.band, bit.bor, bit.rol
local lshift, rshift  = bit.lshift, bit.rshift

local bset = function (x,n) return bor (x, rol( 1, n))      end
local btst = function (x,n) return band(x, rol( 1, n)) ~= 0 end
local bclr = function (x,n) return band(x, rol(-2, n))      end
local lt   = function (x,y) return x <  y                   end
local le   = function (x,y) return x <= y                   end
local gt   = function (x,y) return x >  y                   end
local ge   = function (x,y) return x >= y                   end

-- metatables & metamethods

local ffi = require 'ffi'

local metaname = {
  '__add', '__call', '__concat', '__copy', '__div', '__eq', '__exec', '__gc',
  '__index', '__init', '__ipairs', '__le', '__len', '__lt', '__metatable',
  '__mod', '__mode', '__mul', '__new', '__newindex', '__pairs', '__pow',
  '__same', '__sub', '__tostring', '__totable', '__unm',
}
for _,v in ipairs(metaname) do metaname[v] = v end -- build dict of metaname

local function invalid_use (a)
  error("invalid use of object <" .. tostring(a) .. ">", 2)
end

local function get_metatable (a)
  return type(a) == 'cdata' and (a.__metatable or ffi.miscmap[-tonumber(ffi.typeof(a))])
         or getmetatable(a)
end

local function has_metatable (a)
  return not not get_metatable(a)
end

local function get_metamethod (a, f)
  local mt = get_metatable(a)
  return mt and rawget(mt,f)
end

local function has_metamethod (a, f)
  local mt = get_metatable(a)
  local mm = mt and rawget(mt,f)
  return not not mm and mm ~= invalid_use
end

local function has_metamethod_ (a, f)
  local mt = get_metatable(a)
  local mm = mt and rawget(mt,f)
  return not not mm -- false or nil -> false
end

-- types

local typeclass = {
  -- false
  table = false, lightuserdata = false, userdata = false, cdata = false,
  -- true
  ['nil'] = true,  boolean = true, number = true, string = true,
  -- nil
  ['function'] = nil, thread = nil,
}

local function is_nil (a)
  return type(a) == 'nil'
end

local function is_boolean (a)
  return type(a) == 'boolean'
end

local function is_number (a)
  return type(a) == 'number'
end

local function is_string (a)
  return type(a) == 'string'
end

local function is_function (a)
  return type(a) == 'function'
end

local function is_table (a)
  return type(a) == 'table'
end

local function is_rawtable (a)
  return type(a) == 'table' and rawequal(getmetatable(a), nil)
end

local function is_emptytable(a)
  return type(a) == 'table'  and rawequal(next(a), nil)
end

local function is_metaname (a)
  return metaname[a] == a
end

local function is_value(a)
  return typeclass[type(a)]
end

local function is_stringable(a)
  return has_metamethod_(a,'__tostring')
end

-- concepts

local function is_callable (a)
  return type(a) == 'function' or has_metamethod_(a,'__call')
end

local function is_iterable (a)
 return type(a) == 'table' or has_metamethod (a,'__ipairs')
end

local function is_mappable (a)
 return type(a) == 'table' or has_metamethod (a,'__pairs')
end

-- iterator: pairs = ipairs + kpairs

local function kpairs_iter (tbl, key)
  local k, v = key
  repeat k, v = tbl.nxt(tbl.dat, k)                -- discard ipairs indexes
  until type(k) ~= 'number' or (k%1) ~= 0 or k > tbl.lst or k < 1
  return k, v
end

local function kpairs (tbl, lst_)
  assert(is_mappable(tbl), "invalid argument #1 (mappable expected)")
  local nxt, dat, ini = pairs(tbl)
  local lst = lst_
  if not lst and is_iterable(tbl) then
    for i in ipairs(tbl) do lst = i end
  end
  if not lst then return nxt, dat, ini end
  return kpairs_iter, { nxt=nxt, dat=dat, lst=lst }, ini
end

-- extensions (conversion, factory)

local tostring_ -- forward ref

local function tbl2str (tbl, sep_)
  assert(is_mappable(tbl), "invalid argument #1 (mappable expected)")
  if is_emptytable(tbl) then return '{}' end
  local r = {}
  for i,v in ipairs(tbl) do
    r[i] = tostring_(v)
  end
  local ir = #r+1
  for k,v in kpairs(tbl) do
    r[ir], ir = '['..tostring_(k)..']='..tostring_(v), ir+1
  end
  return '{'..table.concat(r, sep_ or ', ')..'}'
end

function tostring_ (a, ...)      -- to review
  if     is_string(a)     then return a
  elseif is_value(a)      then return (tostring(a))
  elseif is_stringable(a) then return (get_metamethod(a,'__tostring')(a, ...))
  elseif is_mappable(a)   then return tbl2str(a,...) -- table.concat
  else                         return (tostring(a)) -- builtin tail call...
  end
end

-- searching

local function bsearch (tbl, val, cmp_, low_, high_)
  assert(is_iterable(tbl), "invalid argument #1 (iterable expected)")
  if is_number(cmp_) and is_nil(high_) then
    cmp_, low_, high_ = nil, cmp_, low_ -- right shift
  end
  if not (is_nil(cmp_) or is_callable(cmp_)) then
    error("invalid argument #3 (callable expected)")
  end
  local cmp  = cmp_ or lt -- cmp must be the same used by table.sort
  local low  = is_number(low_ ) and max(low_ , 1   ) or 1
  local high = is_number(high_) and min(high_, #tbl) or #tbl
  local cnt, mid, stp, tst = high-low+1
  while cnt > 0 do
    stp = rshift(cnt,1)
    mid = low+stp
    tst = cmp(tbl[mid], val)
    low = tst and mid+1 or low
    cnt = tst and rshift(cnt-1,1) or stp
  end
  return low -- tbl[low] <= val
end

-- functors

local _fun, _fun2 = {}, {}
local fct_mt, fct_mtc

local function functor (f)
  assert(is_callable(f), "invalid argument #1 (callable expected)")
  return setmetatable({[_fun]=f}, fct_mt)
end

local function compose (f, g)
  assert(is_callable(f), "invalid argument #1 (callable expected)")
  assert(is_callable(g), "invalid argument #2 (callable expected)")
  return setmetatable({[_fun]=f, [_fun2]=g}, fct_mtc)
end

local function is_functor (a)
  return is_table(a) and rawget(a,_fun) ~= nil
end

local str = function(s) return string.format("functor: %p", s) end
local err = function()  error("forbidden access to functor", 2) end

fct_mt = {
  __pow       = compose,
  __call      = function(s,...) return rawget(s,_fun)(...) end,
  __index     = function(s,k)   return s(k)                end,
  __tostring  = str,
  __len = err, __newindex = err, __ipairs = err, __pairs = err,
}

fct_mtc = {
  __pow       = compose,
  __call      = function(s,...) return rawget(s,_fun)(rawget(s,_fun2)(...)) end,
  __index     = function(s,k)   return s(k)                                 end,
  __tostring  = str,
  __len = err, __newindex = err, __ipairs = err, __pairs = err,
}

-- implementation -------------------------------------------------------------o

local M = {}

-- Root of all objects, forward declaration
local object

-- object members
local _var = {} -- hidden key

-- reserved flags (bits)
local _flg = {} -- hidden key
local flg_ro, flg_cl = 0, 1 -- flags id for readonly and class
local flg_free = 2          -- used flags (0 - 1), free flags (2 - 31)

-- instance and metatable of 'incomplete objects' proxy
local var0 = setmetatable({}, {
  __index     = function() error("forbidden read access to incomplete object" , 2) end,
  __newindex  = function() error("forbidden write access to incomplete object", 2) end,
  __metatable = false,
})

-- helpers

local function name (a)
  local var = rawget(a,_var)
  return rawget(var,'__id') or ('? <: ' .. var.__id)
end

local function init (a)
  local init = rawget(getmetatable(a), '__init')
  if init then return init(a) end
  return a
end

local function parent (a)
  return getmetatable(rawget(a,'__index'))
end

local function fclass (a)
  return btst(rawget(a,_flg) or 0, flg_cl)
end

local function freadonly (a)
  return btst(rawget(a,_flg) or 0, flg_ro)
end

local function set_class (a)
  rawset(a,_flg, bset(rawget(a,_flg) or 0, flg_cl))
  return a
end

local function is_object (a) -- exported
  return is_table(a) and rawget(a,_var) ~= nil
end

local function is_class (a) -- exported
  return is_table(a) and fclass(a)
end

local function is_instanceOf (a, b) -- exported
  if is_object(a) and is_class(b) then
    repeat a = parent(a) until not a or rawequal(a,b)
    return not not a
  end
  return false
end

-- metamethods

local MT = {}

-- objects are proxies controlling variables access and inheritance
function MT:__call (a, b) -- object constructor (define the object-model)
  if is_string(a) or is_nil(a) then                     -- [un]named object
    if is_nil(b) then
      local obj = {__id=a, [_var]=var0, __index=rawget(self,_var)} -- proxy
      return setmetatable(obj, getmetatable(self))      -- incomplete object
    elseif is_rawtable(b) then
      local obj = {[_var]=b, __index=rawget(self,_var)} -- proxy
      b.__id=a ; setmetatable(b, obj) ; set_class(self) -- set fast inheritance
      return init(setmetatable(obj, getmetatable(self)))-- complete object
    end
  elseif is_rawtable(a) then
    if rawget(self,_var) == var0 then                   -- finalize named object
      a.__id, self.__id = self.__id, nil
      rawset(self,_var, setmetatable(a, self));         -- set fast inheritance
      set_class(parent(self))
      return init(self)
    else                                                -- unnamed object
      local obj = {[_var]=a, __index=rawget(self,_var)} -- proxy
      setmetatable(a, obj) ; set_class(self)            -- set fast inheritance
      return init(setmetatable(obj, getmetatable(self)))-- complete object
    end
  end
  error(is_nil(b) and "invalid argument #1 (string or raw table expected)"
                  or  "invalid argument #2 (raw table expected)", 2)
end

local function raw_len (self)
  return rawlen(rawget(self,_var))    -- no inheritance
end

local function raw_get (self, k)
  return rawget(rawget(self,_var),k)  -- no inheritance nor function evaluation
end

local function raw_set (self, k, v)
  rawset(rawget(self,_var), k, v)     -- no protection!!
end

local function var_raw (self, k)
  return rawget(self,_var)[k]         -- no function evaluation with inheritance
end

local function var_val (self, k, v)   -- string key with value function
  if type(k) == 'string' and type(v) == 'function'
  then return v(self)
  else return v end
end

local function var_get (self, k) -- reusing var_raw and var_val kills the inlining
  local v = rawget(self,_var)[k]
  if type(k) == 'string' and type(v) == 'function'
  then return v(self)
  else return v end
end

function MT:__index (k)          -- reusing var_raw and var_val kills the inlining
  local v = rawget(self,_var)[k]
  if type(k) == 'string' and type(v) == 'function'
  then return v(self)
  else return v end
end

function MT:__newindex (k, v)
  if freadonly(self) or type(k) == 'string' and string.sub(k,1,2) == '__' then
    error("forbidden write access to '" .. name(self) ..
          "' (readonly object or variable)", 2)
  end
  rawget(self,_var)[k] = v      -- note: must use [k] for var0
end

function MT:__len ()
  local var = rawget(self,_var)
  if is_nil(var[1]) then return 0 end -- fast
  while is_nil(rawget(var,1)) do      -- slow
    var  = rawget(self,'__index')
    self = getmetatable(var)
  end
  return rawlen(var)
end

local function iter (var, key) -- scan only numbers and strings
  local k, v = next(var, key)
  while type(k) ~= 'string' and type(k) ~= 'number' and k do
    k, v = next(var, k)
  end
  return k, v
end

local function pairs_iter (self)
  return iter, rawget(self,_var), nil
end

local function ipairs_iter (self)
  return ipairs(rawget(self,_var))
end

MT.__pairs  =  pairs_iter
MT.__ipairs = ipairs_iter

function MT:__tostring()
  return string.format("object: '%s' %p", name(self), self)
end

-- methods

local function is_readonly (self)
  assert(is_object(self), "invalid argument #1 (object expected)")
  return freadonly(self)
end

local function set_readonly (self, set_)
  assert(is_object(self), "invalid argument #1 (object expected)")
  if set_ ~= false
  then rawset(self, _flg, bset(rawget(self,_flg) or 0, flg_ro))
  else rawset(self, _flg, bclr(rawget(self,_flg) or 0, flg_ro))
  end
  return self
end

local function get_variables (self, lst, noeval_)
  assert(is_object(self) , "invalid argument #1 (object expected)")
  assert(is_iterable(lst), "invalid argument #2 (iterable expected)")
  local n   = #lst
  local res = table.new(0,n)
  local get = noeval_ == true and var_raw or var_get
  for i=1,n do res[lst[i]] = get(self, lst[i]) end
  return res -- key -> val
end

local function set_variables (self, tbl, override_)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(is_mappable(tbl)   , "invalid argument #2 (mappable expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  local var = rawget(self,_var)
  local id  = rawget(var,'__id')
  for k,v in pairs(tbl) do
    assert(is_nil(rawget(var,k)) or override_ ~= false, "cannot override variable")
    var[k] = v
  end
  var.__id = id
  return self
end

local function wrap_variables (self, tbl)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(is_mappable(tbl)   , "invalid argument #2 (mappable expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  local var = rawget(self,_var)
  local id  = rawget(var,'__id')
  for k,f in pairs(tbl) do
    local v, newv = var[k]
    assert(not is_nil(v) , "invalid variable (nil value)")
    assert(is_callable(f), "invalid wrapper (callable expected)")
    if is_callable(v) then
      newv = f(v)
    else
      local fv = function() return v end
      newv = f(fv)
    end -- simplify user's side.
    if is_functor(v) and not is_functor(newv) then
      newv = functor(newv)                   -- newv must maintain v's semantic.
    end
    var[k] = newv
  end
  var.__id = id
  return self
end

local function set_methods (self, tbl, override_, strict_)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(is_mappable(tbl)   , "invalid argument #2 (mappable expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  local var = rawget(self,_var)
  local id  = rawget(var,'__id')
  for k,f in pairs(tbl) do
    assert(is_string(k), "invalid key (function name expected)")
    assert(is_callable(f) or strict_ == false, "invalid value (callable expected)")
    assert(is_nil(rawget(var,k)) or override_ ~= false, "cannot override function")
    var[k] = is_function(f) and functor(f) or f
  end
  var.__id = id
  return self
end

local function set_metamethods (self, tbl, override_, strict_)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(is_mappable(tbl)   , "invalid argument #2 (mappable expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  local sm, pm = getmetatable(self), getmetatable(parent(self)) or MT
  if sm == pm then -- create new metatable if same as parent
    assert(not fclass(self), "invalid metatable write access (unexpected class)")
    sm=table.new(0,8) for k,v in pairs(pm) do sm[k] = v end
    pm.__metatable = nil -- unprotect change
    setmetatable(self, sm)
    pm.__metatable, sm.__metatable = pm, sm
  end
  for k,mm in pairs(tbl) do
    assert(is_metaname(k) or strict_ == false, "invalid key (metamethod expected)")
    assert(is_nil(rawget(sm,k)) or override_ == true, "cannot override metamethod")
    sm[k] = mm
  end
  return self
end

local function final_err (self)
  error("invalid object creation ('"..name(self).."' is qualified as final)", 2)
end

local function set_final (self)
  return set_metamethods(self, {__call=final_err}, true)
end

local function get_varkeys (self, class_)
  assert(is_object(self)                    , "invalid argument #1 (object expected)")
  assert(is_nil(class_) or is_object(class_), "invalid argument #2 (object expected)")
  local lst, key = table.new(8,1), table.new(0,8)
  while self and not rawequal(self, class_) do
    for k,v in kpairs(self) do
      if not (key[k] or is_functor(v)) and is_string(k) and string.sub(k,1,2) ~= '__'
      then lst[#lst+1], key[k] = k, k
      end
    end
    self = parent(self)
  end
  assert(rawequal(self, class_),"invalid argument #2 (parent of argument #1 expected)")
  return lst
end

local function insert (self, idx_, val)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  table.insert(rawget(self,_var), idx_, val)
  return self
end

local function remove (self, idx_)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  return table.remove(rawget(self,_var), idx_)
end

local function move (self, idx1, idx2, idxto, dest_)
  dest_ = dest_ or self
  assert(is_object(self)     , "invalid argument #1 (object expected)")
  assert(is_object(dest_)    , "invalid argument #2 (object expected)")
  assert(not freadonly(dest_), "forbidden write access to readonly object")
  table.move(rawget(self,_var), idx1, idx2, idxto, rawget(dest_,_var))
  return dest_
end

local function sort (self, cmp_)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  table.sort(rawget(self,_var), cmp_)
  return self
end

local function bsearch_ (self, val, cmp_, low_, high_)
  assert(is_object(self), "invalid argument #1 (object expected)")
  return bsearch(rawget(self,_var), val, cmp_, low_, high_)
end

local function lsearch_ (self, val, equ_, low_, high_)
  assert(is_object(self), "invalid argument #1 (object expected)")
  return lsearch(rawget(self,_var), val, equ_, low_, high_)
end

local function clear_array (self)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  local var = rawget(self,_var)
  for i=1,rawlen(var) do var[i]=nil end
  return self
end

local function clear_variables (self)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  local var = rawget(self,_var)
  local id  = rawget(var,'__id')
  for k in kpairs(self) do var[k]=nil end
  var.__id = id
  return self
end

local function clear_all (self)
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(not freadonly(self), "forbidden write access to readonly object")
  local var = rawget(self,_var)
  local id  = rawget(var,'__id')
  for k in pairs_iter(self) do var[k]=nil end -- table.clear destroys all keys
  var.__id = id
  return self
end

-- inheritance

local function set_parent (self, newp)
  assert(is_object(self), "invalid argument #1 (object expected)")
  assert(is_object(newp), "invalid argument #2 (object expected)")
  if freadonly(self) then
    error("forbidden write access to readonly object '" .. name(self) .. "'", 2)
  end
  local spar = self.parent
  if getmetatable(newp) ~= getmetatable(spar) then
    error("new and current parent do not share same metamethods")
  end
  if newp.parent ~= spar.parent then
    error("new and current parent do not inherit from same direct parent")
  end
  rawset(self,'__index', rawget(newp,_var))
  set_class(newp)
  return self
end

-- copy

local function same (self, name_)
  assert(is_object(self)                  ,"invalid argument #1 (object expected)")
  assert(is_nil(name_) or is_string(name_),"invalid argument #2 (string expected)")
  -- same shares the same parent
  local par = parent(self)
  local sam = par(name_, {})
  -- metatable
  local sm, pm = getmetatable(self), getmetatable(par)
  if sm ~= pm then -- copy metatable
    local cm=table.new(0,8) for k,v in pairs(sm) do cm[k] = v end
    sm.__metatable = nil
    setmetatable(sam, cm)
    sm.__metatable, cm.__metatable = sm, cm
  end
  return sam
end

local function copy (self, name_)
  assert(is_object(self)                  ,"invalid argument #1 (object expected)")
  assert(is_nil(name_) or is_string(name_),"invalid argument #2 (string expected)")
  local cpy = same(self, name_ or raw_get(self,'__id'))
  local var, cvar = rawget(self,_var), rawget(cpy,_var)
  local id  = rawget(cvar,'__id')
  for k,v in pairs_iter(self) do cvar[k] = v end
  cvar.__id = id
  return cpy
end

MT.__same = same
MT.__copy = copy

-- flags

local flg_mask = lshift(-1, flg_free)
local flg_notmask = bnot(flg_mask)

local function test_flag (self, n)
  assert(is_object(self), "invalid argument #1 (object expected)")
  assert(is_number(n)   , "invalid argument #2 (number expected)")
  return btst(rawget(self,_flg) or 0, n)
end

local function set_flag (self, n)
  assert(is_object(self), "invalid argument #1 (object expected)")
  assert(is_number(n)   , "invalid argument #2 (number expected)")
  if n >= flg_free then
    rawset(self, _flg, bset(rawget(self,_flg) or 0, n))
  end
  return self
end

local function clear_flag (self, n)
  assert(is_object(self), "invalid argument #1 (object expected)")
  assert(is_number(n)   , "invalid argument #2 (number expected)")
  if n >= flg_free then
    rawset(self, _flg, bclr(rawget(self,_flg) or 0, n))
  end
  return self
end

local function get_flags (self)
  assert(is_object(self), "invalid argument #1 (object expected)")
  return rawget(self,_flg) or 0
end

local function test_flags (self, flags)
  assert(is_object(self) , "invalid argument #1 (object expected)")
  assert(is_number(flags), "invalid argument #2 (number expected)")
  return band(rawget(self,_flg) or 0, flags) ~= 0
end

local function set_flags (self, flags)
  assert(is_object(self) , "invalid argument #1 (object expected)")
  assert(is_number(flags), "invalid argument #2 (number expected)")
  flags = band(flags, flg_mask)
  rawset(self, _flg, bor(rawget(self, _flg) or 0, flags))
  return self
end

local function clear_flags (self, flags)
  assert(is_object(self) , "invalid argument #1 (object expected)")
  assert(is_number(flags), "invalid argument #2 (number expected)")
  flags = band(flags, flg_mask)
  rawset(self, _flg, band(rawget(self, _flg) or 0, bnot(flags)))
  return self
end

-- environments

local _env = {} -- hidden key

local function open_env (self, ctx_)
  assert(is_object(self), "invalid argument #1 (object expected)")
  assert(is_nil(ctx_) or is_function(ctx_) or is_number(ctx_) and ctx_ >= 1,
                          "invalid argument #2 (not a function or < 1)")
  ctx_ = is_function(ctx_) and ctx_ or is_number(ctx_) and ctx_+1 or 2
  assert(is_nil(rawget(self,_env)), "invalid environment (already open)")
  rawset(self, _env, { ctx=ctx_, env=getfenv(ctx_) })
  rawset(self, self.__id, self) -- self reference
  setfenv(ctx_, self)
  return self
end

local function is_open_env (self)
  assert(is_object(self), "invalid argument #1 (object expected)")
  return not is_nil(rawget(self,_env))
end

local function reset_env (self) -- if an error occurs while in the environment
  assert(is_object(self), "invalid argument #1 (object expected)")
  rawset(self, _env, nil)
  rawset(self, self.__id, nil) -- clear self reference
  return self
end

local function close_env (self)
  assert(is_object(self), "invalid argument #1 (object expected)")
  local env = rawget(self,_env)
  assert(not is_nil(env), "invalid environment (not open)")
  setfenv(env.ctx, env.env)
  return reset_env(self)
end

local function dump_env (self) -- for debug
  for k,v in pairs(rawget(self,_var)) do
    if is_rawtable(v) then
      for k,v in pairs(v) do
        print(k,'=',v)
      end
    elseif is_object(v) then
      print(k,'=',name(v))
    else
      print(k,'=',v)
    end
  end
end

-- I/O ------------------------------------------------------------------------o

-- dump obj members (including controlled inheritance)
local function dumpobj (self, filnam_, class_, pattern_)
  if is_object(filnam_) and is_nil(pattern_) then
    filnam_, class_, pattern_ = nil, filnam_, class_ -- right shift
  end
  if is_string(class_) and is_nil(pattern_) then
    class_, pattern_ = nil, class_                   -- right shift
  end

  class_, pattern_ = class_ or object, pattern_ or ''
  assert(is_object(self)    , "invalid argument #1 (object expected)")
  assert(is_object(class_)  , "invalid argument #3 (object expected)")
  assert(is_string(pattern_), "invalid argument #4 (string expected)")

  local tostring = tostring_
  local n, cnt, res, spc, str = 0, {}, {}, ""
  while self and not rawequal(self, class_) do
    local var = rawget(self,_var)
    -- header
    local id = rawget(var, '__id')
    n, str = n+1, id and (" '" .. id .. "'") or ""
    res[n] = spc .. "+ " .. tostring(self)
    spc = spc .. "   "
    -- variables
    for k,v in kpairs(self) do
      if is_string(k) and string.sub(k,1,2) ~= '__' and string.find(k,pattern_) then
        str = spc .. tostring(k)
        if is_string(v) then
          str = str .. " : '" .. tostring(v):sub(1,15) .. "'"
        elseif is_function(v) then
          str = str .. " := " .. tostring(v(self))
        else
          str = str .. " :  " .. tostring(v)
        end
        if cnt[k]
        then str = str .. " (" .. string.rep('*', cnt[k]) .. ")" -- mark overrides
        else cnt[k] = 0
        end
        cnt[k], n = cnt[k]+1, n+1
        res[n] = str
      end
    end
    self = parent(self)
  end
  assert(rawequal(self, class_), "invalid argument #2 (parent of argument #1 expected)")

  -- return result as a string
  if filnam_ == '-' then return table.concat(res, '\n') end

  -- dump to file
  local file = openfile(filnam_, 'w', '.dat')
  for _,s in ipairs(res) do file:write(s,'\n') end
  if is_string(filnam_) then file:close() else file:flush() end

  return self
end

-- members --------------------------------------------------------------------o

M.__id  = 'object'
M.__par = parent
M.first_free_flag = flg_free

-- methods
M.is_class        = functor( is_class        )
M.is_readonly     = functor( is_readonly     )
M.is_instanceOf   = functor( is_instanceOf   )

M.set_parent      = functor( set_parent      )
M.set_readonly    = functor( set_readonly    )
M.set_final       = functor( set_final       )

M.get_varkeys     = functor( get_varkeys     )
M.get_variables   = functor( get_variables   )
M.set_variables   = functor( set_variables   )
M.wrap_variables  = functor( wrap_variables  )

M.set_methods     = functor( set_methods     )
M.set_metamethods = functor( set_metamethods )

M.insert          = functor( insert          )
M.remove          = functor( remove          )
M.move            = functor( move            )
M.sort            = functor( sort            )
M.bsearch         = functor( bsearch_        )
M.lsearch         = functor( lsearch_        )
M.clear_array     = functor( clear_array     )
M.clear_variables = functor( clear_variables )
M.clear_all       = functor( clear_all       )

M.same            = functor( same            )
M.copy            = functor( copy            )

M.raw_len         = functor( raw_len         )
M.raw_get         = functor( raw_get         )
M.raw_set         = functor( raw_set         )

M.var_raw         = functor( var_raw         )
M.var_val         = functor( var_val         )
M.var_get         = functor( var_get         )

M.set_flag        = functor( set_flag        )
M.test_flag       = functor( test_flag       )
M.clear_flag      = functor( clear_flag      )
M.get_flags       = functor( get_flags       )
M.set_flags       = functor( set_flags       )
M.test_flags      = functor( test_flags      )
M.clear_flags     = functor( clear_flags     )

M.open_env        = functor( open_env        )
M.reset_env       = functor( reset_env       )
M.close_env       = functor( close_env       )
M.is_open_env     = functor( is_open_env     )

M.dumpobj         = functor( dumpobj         )

-- aliases
M.parent = parent
M.name   = function(s) return s.__id end
M.set    = M.set_variables
M.get    = M.get_variables

-- metatables -----------------------------------------------------------------o

-- root object variables = module
object = setmetatable({[_var]=M}, MT)

 -- parent link
setmetatable(M, object)

-- protect against changing metatable
MT.__metatable = MT

-- set as readonly
object:set_readonly()

return {
  object        = object,
  is_object     = is_object,
  is_class      = is_class,
  is_instanceOf = is_instanceOf,
  is_function   = is_function,
  is_table      = is_table,
  is_number     = is_number,
  is_functor    = is_functor,
  tobit         = tobit,
  lt            = lt,
  le            = le,
  ge            = ge,
  gt            = gt,
}
