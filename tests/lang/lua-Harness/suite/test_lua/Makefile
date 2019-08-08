
LUA ?= lua

export LUA_INIT=--[[ coverage ]]

test:
	prove --exec="$(LUA)" *.t

harness:
	prove --exec="$(LUA)" *.t

sanity:
	prove --exec="$(LUA)" 0*.t
