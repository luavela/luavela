-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local tables = {
	{ -- empty table
		t = {},
		keys_as_values = {},
		values_as_values = {},
	},
	{ -- array with zero index
		t = {[0] = 2},
		keys_as_values = {0},
		values_as_values = {2},
	},
	{ -- array with negative indices
		t = {[-1] = 3},
		keys_as_values = {-1},
		values_as_values = {3},
	},
	{ -- array with holes
		t = {5, nil, 3, nil, 2, nil, nil, nil, {[1] = 5}},
		keys_as_values = {1, 3, 5, 9},
		values_as_values = {5, 3, 2, {[1] = 5}}, -- unclear how to compare tables (recursively?)
	},
	{ -- array with integer values only
		t = {1, 2, 3, 4},
		keys_as_values = {1, 2, 3, 4},
		values_as_values = {1, 2, 3, 4},
	},
	{ -- array with various values
		t = {5, 3, 2, {[1] = 5}},
		keys_as_values = {1, 2, 3, 4},
		values_as_values = {5, 3, 2, {[1] = 5}},
	},
	{ -- hash
		t = {abc = 1, W = {[1] = 39}, def = 77623, iou = 8876, rew = 7, ww = 33},
		keys_as_values = {"abc", "W", "def", "iou", "rew", "ww"},
		values_as_values = {1, {[1] = 39}, 77623, 8876, 7, 33},
	},
	{ -- mixed
		t = {1, {[1] = 2}, abc = "gtr", def = {[1] = 387}},
		keys_as_values = {1, 2, "abc", "def"},
		values_as_values = {1, {[1] = 2}, "gtr", {[1] = 387}},
	},
	{ -- mixed with holes
		t = {1, 2, ["asdas"] = 99, 4, nil, nil, nil, 5, nil, 9},
		keys_as_values = {1, 2, 3, 7, 9, "asdas"},
		values_as_values = {1, 2, 99, 4, 5, 9},
	},
	{ -- mixed with keys from array part going not in succession.
	  -- NB: keys order in array part may be _undefined_.
		t = {1, ["key"] = 99, 9},
		keys_as_values = {1, 2, "key"},
		values_as_values = {1, 99, 9},
	},
	{ -- mixed with metatable
		t = {1, abc = "we"},
		keys_as_values = {1, "abc"},
		values_as_values = {1, "we"},
		mt = {__index = {mt_key = 34}},
	}
}

return {
	tables = tables
}
