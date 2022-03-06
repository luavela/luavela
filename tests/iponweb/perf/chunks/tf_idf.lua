-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
jit.opt.start("jitpairs")

local corpus = require('tf_idf_corpus')

local function assert_almost_equal(test, expected)
  assert(math.abs(test - expected) < 0.01)
end

local function split(text)
  local words = {}
  for w in text:gmatch("%S+") do
    table.insert(words, w)
  end
  return words
end

local function bag(words)
  local b = {}
  for _, word in pairs(words) do
    if b[word] == nil then
      b[word] = 0
    end
    b[word] = b[word] + 1
  end
  return b
end

local function tf(b, sum)
  local f = {}
  for word, count in pairs(b) do
    f[word] = count / sum
  end
  return f
end

local function new(words)
  local obj = {}
  obj.words = words
  obj.sum = #words
  obj.tf = tf(bag(words), obj.sum)
  return obj
end

local function count(tab)
  local c = 0
  for _ in pairs(tab) do
    c = c + 1
  end
  return c
end

-- preprocessing
for title, text in pairs(corpus) do
  corpus[title] = split(text)
end

local idf = {}
local tf_idf = {}
local data = {}
local word_to_docs = {}
local log = math.log

local N = arg[1] or 1e3
for _ = 1, N do
  for name, words in pairs(corpus) do
    data[name] = new(words)
  end
  local data_count = count(data)

  for name, obj in pairs(data) do
    for _, word in pairs(obj.words) do
      if word_to_docs[word] == nil then
        word_to_docs[word] = {}
      end
      word_to_docs[word][name] = true
    end
  end
  for word, docs in pairs(word_to_docs) do
    idf[word] = log(data_count / count(docs))
  end
  for name, obj in pairs(data) do
    tf_idf[name] = {}
    for word, f in pairs(obj.tf) do
      tf_idf[name][word] = f * idf[word]
    end
  end
end
assert_almost_equal(idf['boy'], 2.3025)
assert_almost_equal(tf_idf['The Nest-Builder']['irish'], 0.0024)
