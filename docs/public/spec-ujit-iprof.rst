.. _spec-ujit-iprof:

|PROJECT| ujit.iprof: Platform-level Instrumenting Profiler for Lua Code
========================================================================

Purpose
-------

This document is a specification for |PROJECT| instrumenting profiler usage
including usage examples describing its features and limitations.

Target Audience
---------------

Lua developers and anyone interested in Lua code performance assessment.

Interface: ``ujit.iprof``
-------------------------

|PROJECT| currently provides only Lua-level API for instrumenting profiling,
however, statistics can be passed via callback either provided by platform,
within |PROJECT| is integrated or written in pure Lua (see some examples below).
The resulting table will contain up to four values per each profiled entity:

- ``calls`` -- how many times profiled entity has been called.
- ``wall`` -- processing time for profiled entity (aka wall clock).
- ``lua`` -- total time spent within profiled Lua coroutine.
- ``subs`` -- a table with callees of profiled entity. Omitted if no
  callees within.

``ujit.iprof.start``
^^^^^^^^^^^^^^^^^^^^

Synopsis
""""""""

.. code::

  local status, errmsg = ujit.iprof.start(label, mode, limit)

``ujit.iprof.start`` sets up resources for instrumenting profiler and puts
interpreter into profiling mode.

Arguments
"""""""""

1. **label** (required) – name of profiling entity.
2. **mode** (optional) – profile mode from values listed below (default is
   ``ujit.iprof.PLAIN``).

  - ``ujit.iprof.PLAIN`` -- counts inclusive time only for profiled lines.

  - ``ujit.iprof.INCLUSIVE`` -- counts inclusive time for profiled lines and all
    nested callees up to the ``limit`` depth.

  - ``ujit.iprof.EXCLUSIVE`` -- counts exclusive (self) time for profiled lines
    and all nested callees up to the ``limit`` depth.

3. **limit** (optional) – max Lua frame depth to be profiled (default is
   ``LJ_MAX_XLEVEL``).

Return value
""""""""""""

In case of errors ``ujit.iprof.start`` returns **false** with the
corresponding error message. Otherwise it returns **true**.

Errors
""""""

1. ``"wrong profiling mode, use ujit.iprof.{PLAIN,INCLUSIVE,EXLCUSIVE}"`` --
   error raised when profiling mode parameter is out of acceptable set.
2. ``"wrong profiling limit, use non-negative number less than <number>"`` --
   error raised when profiling limit is out of acceptable range.

3. ``"Inappropriate ujit build"`` -- error raised while using profiler
   interfaces when profiler is not available.

Also there are two errors that can be removed in future:

4. ``"JIT is enabled"`` -- error raised while using profiler with enabled jit.
   The reason of this behaviour is described below.
5. ``"Error occured while profiling"`` -- error raised for the case of nested
   profiling. The reason of this behaviour is described below.

``ujit.iprof.stop``
^^^^^^^^^^^^^^^^^^^

Synopsis
""""""""

.. code::

  local result, errmsg = ujit.iprof.stop()

``ujit.iprof.stop`` releases resources being allocated for profiling and puts
interpreter out of profiling mode.

Arguments
"""""""""

No arguments are required for this function.

Return value
""""""""""""

If there are no errors, ``ujit.iprof.stop`` returns a **table** with profiler
results. Otherwise it returns **nil**, plus the error message.

Errors
""""""

1. ``"Inappropriate ujit build"`` -- error raised while using profiler
   interfaces when profiler is not available.
2. ``"Error occured while profiling"`` -- error raised while calling
   ``ujit.iprof.stop`` with no prior ``ujit.iprof.start`` call.

``ujit.iprof.profile``
^^^^^^^^^^^^^^^^^^^^^^

This subroutine is not provided directly by uJIT considering two following
problems we faced while implementing ``ujit.iprof.profile``:

  * Coroutine isn't able to yield through C function (platform design limit).
  * Today there is no way to implement |PROJECT| builtin in pure Lua.

Thereby we can just propose an interface and an example of pure Lua
implementation you can embed to your platform/profiling suite/etc (see Notes
section below).

Synopsis
""""""""

.. code::

  local wpfunction = ujit.iprof.profile(pfunction, pcallback,
                                        label, mode, limit)

``ujit.iprof.profile`` is a wrapper for function to be profiled with
``ujit.iprof.start`` and ``ujit.iprof.stop`` underneath.

Arguments
"""""""""

1. **pfunction** (required) – function to be profiled
2. **pcallback** (required) – function expecting table with profiler results as
   a first argument.
3. **label** (optional) -- the same as ``ujit.iprof.start`` **label** argument
4. **mode** (optional) -- the same as ``ujit.iprof.start`` **mode** argument
5. **limit** (optional) -- the same as ``ujit.iprof.start`` **limit** argument

Return value
""""""""""""

``ujit.iprof.profile`` returns the wrapped function **wpfunction**. All
profiler-related errors will be passed as argument to **pcallback** with
foregoing **nil**. At the end of wrapped function the profiling is complete with
no errors then the table with profiler stats will be passed to the
``pcallback``.

Errors
""""""

According to ``ujit.iprof.profile`` synopsis all possible errors can be found
within corresponding sections for ``ujit.iprof.start`` and ``ujit.iprof.stop``.

Notes
-----

Here is the list with limitations for current ``ujit.iprof`` implementation:

1. **Nested profiling**: ``ujit.iprof.start`` call when profiling has been
   initiated does not spawn another "profiling frame" for now. There are no
   internal confines related to it in platform, so let me say it's just another
   NYI in |PROJECT|.
2. **Traces profiling**: ``ujit.iprof.start`` obliges JIT to be disabled while
   profiling process.

.. TODO: Add some more lines for JIT

As mentioned before here is an example of proposed wrapper written in pure Lua:

.. code::

  function ujit.iprof.profile(pfunction, pcallback, label, mode, limit)
    return function(...)
       local s, e = ujit.iprof.start(label, mode, limit)
       local t = { pfunction(...) }
       if s then pcallback(ujit.iprof.stop()) else pcallback(nil, e) end
       return table.unpack(t)
    end
  end

Examples
--------

Simple example
^^^^^^^^^^^^^^

Let's try ``ujit.iprof`` with the classic program to start anything in
programming with.

.. code::

  print('Hello, world!')

Everything we need to profile this snippet is to add a directive to start
profiling and a corresponding one to stop it. For a consistency we will also
check that both calls succeeded.

.. code::

  local s, se = ujit.iprof.start('HELLO-IPROF')
  if not s then error(se) end

  print('Hello, world!')

  local r, re = ujit.iprof.stop()
  if not r then error(re) end

The ``r`` variable stores a table with the report produced by profiler, thus
let's dump it with the auxiliary library that can be found
`here <https://github.com/kikito/inspect.lua>`__.

.. code::

  local inspect = require 'inspect'

  local s, se = ujit.iprof.start('HELLO-IPROF')
  if not s then error(se) end

  print('Hello, world!')

  local r, re = ujit.iprof.stop()
  if not r then error(re) end

  print(inspect(r, { indent = '\t' }))

Now we a ready to profile this chunk. Run the command below in terminal.

.. code::

  $ ujit -joff hello-world.lua
  Hello, world!
  {
  	["HELLO-IPROF"] = {
  		calls = 1,
  		lua = 4.4150016037747e-05,
  		wall = 4.4150016037747e-05
  	}
  }

Here you see the desired output followed by the resulting table with the fields
described above.

OK then, we need to go deeper.

Other modes examples
^^^^^^^^^^^^^^^^^^^^

As it was prior mentioned there are three different modes provided for
instrumenting profiler: plain, inclusive and exclusive (for more information
see description above).

So let's just add an optional CLI argument to define profiling mode.

.. code::

  local inspect = require 'inspect'

  local mode = ujit.iprof[arg[1] or 'PLAIN']

  local s, se = ujit.iprof.start('HELLO-IPROF', mode)
  if not s then error(se) end

  print('Hello, world!')

  local r, re = ujit.iprof.stop()
  if not r then error(re) end

  print(inspect(r, { indent = '\t' }))

Let's see what we'll get for the previous command.

.. code::

  $ ujit -joff hello-iprof.lua
  Hello, world!
  {
  	["HELLO-IPROF"] = {
  		calls = 1,
  		lua = 4.2922969441861e-05,
  		wall = 4.2922969441861e-05
  	}
  }

Exactly the same structure, since ``ujit.iprof.start`` second argument defaults
to ``ujit.iprof.PLAIN`` if omitted.

Let's try an inclusive one (mind the uppercase for the mode names).

.. code::

  $ ujit -joff hello-iprof.lua INCLUSIVE
  Hello, world!
  {
  	["HELLO-IPROF"] = {
  		calls = 1,
  		lua = 7.5099989771843e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 9.0009998530149e-06,
  				wall = 9.0009998530149e-06
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 2.0881998352706e-05,
  				wall = 2.0881998352706e-05
  			},
  			["builtin #30"] = {
  				calls = 1,
  				lua = 4.0316954255104e-05,
  				wall = 4.0316954255104e-05
  			}
  		},
  		wall = 7.5099989771843e-05
  	}
  }

Now we see slightly different structure: there is a ``subs`` field now for
``"HELLO-IPROF"`` entity containing three builtin functions:

* ``builtin #223`` -- ``ujit.iprof.start`` internal identifier
* ``builtin #224`` -- ``ujit.iprof.start`` internal identifier
* ``builtin #30`` -- ``print`` internal identifier

Another detail need to be mentioned: lua and processing times for
``"HELLO-IPROF"`` entity include the timings of all its callees. If you want to
see ``"HELLO-IPROF"`` "self" time just change mode to exclusive.

.. code::

  $ ujit -joff hello-iprof.lua EXCLUSIVE
  Hello, world!
  {
  	["HELLO-IPROF"] = {
  		calls = 1,
  		lua = 4.6360655687749e-06,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 9.4409915618598e-06,
  				wall = 9.4409915618598e-06
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.5364959836006e-05,
  				wall = 1.5364959836006e-05
  			},
  			["builtin #30"] = {
  				calls = 1,
  				lua = 4.0235987398773e-05,
  				wall = 4.0235987398773e-05
  			}
  		},
  		wall = 4.6360655687749e-06
  	}
  }

Now all reported timings corresponds directly to each entity.

Function profiling
^^^^^^^^^^^^^^^^^^

The more convenient way to instrument the function itself instead all places it
is used. As mentioned above there is no such functions wrapper provided by
platform but we propose an interface and pure Lua implementation you can find
in this document.

Let's change a bit more our snippet to try ``ujit.iprof.profile`` wrapper.

.. code::

  local inspect = require 'inspect'

  function ujit.iprof.profile(pfunction, pcallback, name, mode, level)
    return function(...)
      local s, e = ujit.iprof.start(name, mode, level)
      local t = { pfunction(...) }
      if s then pcallback(ujit.iprof.stop()) else pcb(nil, e) end
      return table.unpack(t)
    end
  end

  local function stats(t, e)
    if not t then error(e) end
    print(inspect(t, { indent = '\t' }))
  end

  local mode = ujit.iprof[arg[1] or 'PLAIN']
  local wprint = ujit.iprof.profile(print, stats,
                                    'HELLO-IPROF-PROFILE', mode)

  local function echo(s) return s end

  wprint(echo('Hello, world!'))

Except ``ujit.iprof.profile`` we added two additional functions. The first one
-- ``stats`` -- to be invoked when profiler stops and is obliged to have a
special signature considering ``ujit.iprof.stop`` return values. ``stats``
callback throws an error whether profiling was failed and dumps the report table
otherwise. The purpose of the other function -- ``echo`` -- is described below.

So let's test this chunk the way we used to in the latter section.

.. code::

  $ ujit -joff hello-iprof-profile.lua
  Hello, world!
  {
  	["HELLO-IPROF-PROFILE"] = {
  		calls = 1,
  		lua = 4.9892987590283e-05,
  		wall = 4.9892987590283e-05
  	}
  }

.. code::

  $ ujit -joff hello-iprof-profile.lua INCLUSIVE
  Hello, world!
  {
  	["HELLO-IPROF-PROFILE"] = {
  		calls = 1,
  		lua = 7.5692019890994e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 9.3870330601931e-06,
  				wall = 9.3870330601931e-06
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.4016986824572e-05,
  				wall = 1.4016986824572e-05
  			},
  			["builtin #30"] = {
  				calls = 1,
  				lua = 4.0391983930022e-05,
  				wall = 4.0391983930022e-05
  			}
  		},
  		wall = 7.5692019890994e-05
  	}
  }

.. code::

  $ ujit -joff hello-iprof-profile.lua EXCLUSIVE
  Hello, world!
  {
  	["HELLO-IPROF-PROFILE"] = {
  		calls = 1,
  		lua = 1.798098674044e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 1.1770986020565e-05,
  				wall = 1.1770986020565e-05
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 2.080702688545e-05,
  				wall = 2.080702688545e-05
  			},
  			["builtin #30"] = {
  				calls = 1,
  				lua = 5.6924007367343e-05,
  				wall = 5.6924007367343e-05
  			}
  		},
  		wall = 1.798098674044e-05
  	}
  }

And here is the purpose of the ``echo`` function: as you can see there is no
entry for it within all three reports because its execution was finished before
``ujit.iprof.start`` invocation. Reports comparison for profiling approach
provided in previous section with the ``echo`` proxy is left for the reader.

Profiling tail calls
^^^^^^^^^^^^^^^^^^^^

There are some difference in Lua stack layout for plain call (``CALL`` bytecode)
and tail call (``CALLT`` bytecode) while execution and this also reflects on
profiling reports. For advance please read :ref:`tut-lua-calls`.

Here is a code snippet emitting both plain call bytecode and tail call bytecode
and initializing ``q`` according to the arguments passed with CLI.

.. code::

  local inspect = require 'inspect'

  function ujit.iprof.profile(pfunction, pcallback, name, mode, level)
    return function(...)
      local s, e = ujit.iprof.start(name, mode, level)
      local t = { pfunction(...) }
      if s then pcallback(ujit.iprof.stop()) else pcb(nil, e) end
      return table.unpack(t)
    end
  end

  local function stats(t, e)
    if not t then error(e) end
    print(inspect(t, { indent = '\t' }))
  end

  local function qq(str) print(str) end

  local mode = ujit.iprof[arg[1] or 'PLAIN']

  local q = tostring(arg[2]) == 'CALLT'
    and ujit.iprof.profile(function(str) return qq(str) end, stats,
                           'HELLO-IPROF-CALLT', mode)
    or  ujit.iprof.profile(function(str) qq(str) end, stats,
                           'HELLO-IPROF-CALL', mode)

  q('Hello, world!')

As a result of the command below ``q`` will be initialized with the function
containing ``CALL`` + ``RET0`` bytecode combination. For a plain call the
nesting you will see in report is natural: function originated for ``q`` and
defined at ``hello-iprof-callt.lua:24`` includes ``qq`` defined at
``hello-iprof-callt.lua:17``.

.. code::

  $ ujit -joff hello-iprof-callt.lua EXCLUSIVE
  Hello, world!
  {
  	["HELLO-IPROF-CALL"] = {
  		calls = 1,
  		lua = 1.150497701019e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 1.0108051355928e-05,
  				wall = 1.0108051355928e-05
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.6201985999942e-05,
  				wall = 1.6201985999942e-05
  			},
  			["function @hello-iprof-callt.lua:24"] = {
  				calls = 1,
  				lua = 3.4548982512206e-05,
  				subs = {
  					["function @hello-iprof-callt.lua:17"] = {
  						calls = 1,
  						lua = 8.3340564742684e-06,
  						subs = {
  							["builtin #30"] = {
  								calls = 1,
  								lua = 4.7281966544688e-05,
  								wall = 4.7281966544688e-05
  							}
  						},
  						wall = 8.3340564742684e-06
  					}
  				},
  				wall = 3.4548982512206e-05
  			}
  		},
  		wall = 1.150497701019e-05
  	}
  }

The following command will result to ``q`` being initialized with a function
terminating with ``CALLT`` bytecode. Hence according to manipulations with Lua
stack for this bytecode you will see the report containing the origin function
for ``q`` defined at ``hello-iprof-callt.lua:22`` at the same nesting level as
its callee defined at ``hello-iprof-callt.lua:17``.

.. code::

  $ ujit -joff hello-iprof-callt.lua EXCLUSIVE CALLT
  Hello, world!
  {
  	["HELLO-IPROF-CALLT"] = {
  		calls = 1,
  		lua = 1.5244004316628e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 1.1546013411134e-05,
  				wall = 1.1546013411134e-05
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.5365018043667e-05,
  				wall = 1.5365018043667e-05
  			},
  			["function @hello-iprof-callt.lua:17"] = {
  				calls = 1,
  				lua = 1.3436016160995e-05,
  				subs = {
  					["builtin #30"] = {
  						calls = 1,
  						lua = 8.5713982116431e-05,
  						wall = 8.5713982116431e-05
  					}
  				},
  				wall = 1.3436016160995e-05
  			},
  			["function @hello-iprof-callt.lua:22"] = {
  				calls = 1,
  				lua = 4.2876985389739e-05,
  				wall = 4.2876985389739e-05
  			}
  		},
  		wall = 1.5244004316628e-05
  	}
  }

This is another acknowledgment for the remark that profiler describes the way
code is actually executed.

Limiting the profiling depth
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

An attentive reader could notice that ``ujit.iprof.start`` have three arguments
but we still use only two. Let's figure out what **limit** argument is for,
considering the following snippet.

.. code::

  local inspect = require 'inspect'

  function ujit.iprof.profile(pfunction, pcallback, name, mode, level)
    return function(...)
      local s, e = ujit.iprof.start(name, mode, level)
      local t = { pfunction(...) }
      if s then pcallback(ujit.iprof.stop()) else pcb(nil, e) end
      return table.unpack(t)
    end
  end

  local function stats(t, e)
    if not t then error(e) end
    print(inspect(t, { indent = '\t' }))
  end

  function qr(message) print(message) end
  function qx(message) qr(message) end
  function qw(message) qx(message) end
  function qq(message) qw(message) end

  local mode = ujit.iprof[arg[1] or 'PLAIN']
  local limit = tonumber(arg[2] or 42)

  local q = ujit.iprof.profile(function (message) return qq(message) end, stats,
                               'HELLO-IPROF-LIMITS', mode, limit)

  q('Hello, world!')

We add four more functions forming a chain of calls with print in the most inner
one and wrapped a function with tail call to the most outer one. After running
the following commands you will see reports like those below.

.. code::

  $ ujit -joff hello-iprof-limits.lua INCLUSIVE
  Hello, world!
  {
  	["HELLO-IPROF-LIMITS"] = {
  		calls = 1,
  		lua = 0.00013722601579502,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 3.6281999200583e-05,
  				wall = 3.6281999200583e-05
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.7224054317921e-05,
  				wall = 1.7224054317921e-05
  			},
  			["function @hello-iprof-limits.lua:20"] = {
  				calls = 1,
  				lua = 6.3598970882595e-05,
  				subs = {
  					["function @hello-iprof-limits.lua:19"] = {
  						calls = 1,
  						lua = 5.5114971473813e-05,
  						subs = {
  							["function @hello-iprof-limits.lua:18"] = {
  								calls = 1,
  								lua = 4.9913011025637e-05,
  								subs = {
  									["function @hello-iprof-limits.lua:17"] = {
  										calls = 1,
  										lua = 4.4531014282256e-05,
  										subs = {
  											["builtin #30"] = {
  												calls = 1,
  												lua = 3.8045982364565e-05,
  												wall = 3.8045982364565e-05
  											}
  										},
  										wall = 4.4531014282256e-05
  									}
  								},
  								wall = 4.9913011025637e-05
  							}
  						},
  						wall = 5.5114971473813e-05
  					}
  				},
  				wall = 6.3598970882595e-05
  			},
  			["function @hello-iprof-limits.lua:25"] = {
  				calls = 1,
  				lua = 5.9989979490638e-06,
  				wall = 5.9989979490638e-06
  			}
  		},
  		wall = 0.00013722601579502
  	}
  }

.. code::

  $ ujit -joff hello-iprof-limits.lua EXCLUSIVE
  Hello, world!
  {
  	["HELLO-IPROF-LIMITS"] = {
  		calls = 1,
  		lua = 1.368101220578e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 2.4234992451966e-05,
  				wall = 2.4234992451966e-05
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.3143988326192e-05,
  				wall = 1.3143988326192e-05
  			},
  			["function @hello-iprof-limits.lua:20"] = {
  				calls = 1,
  				lua = 8.6590298451483e-06,
  				subs = {
  					["function @hello-iprof-limits.lua:19"] = {
  						calls = 1,
  						lua = 5.8080186136067e-06,
  						subs = {
  							["function @hello-iprof-limits.lua:18"] = {
  								calls = 1,
  								lua = 5.1020178943872e-06,
  								subs = {
  									["function @hello-iprof-limits.lua:17"] = {
  										calls = 1,
  										lua = 5.7529541663826e-06,
  										subs = {
  											["builtin #30"] = {
  												calls = 1,
  												lua = 3.9995997212827e-05,
  												wall = 3.9995997212827e-05
  											}
  										},
  										wall = 5.7529541663826e-06
  									}
  								},
  								wall = 5.1020178943872e-06
  							}
  						},
  						wall = 5.8080186136067e-06
  					}
  				},
  				wall = 8.6590298451483e-06
  			},
  			["function @hello-iprof-limits.lua:25"] = {
  				calls = 1,
  				lua = 6.1789760366082e-06,
  				wall = 6.1789760366082e-06
  			}
  		},
  		wall = 1.368101220578e-05
  	}
  }

No one will argue this report is too complicated for human analysis. Thus you
can limit the profiling depth. Run both previous commands with the second
parameter set to 1.

.. code::

  $ ujit -joff hello-iprof-limits.lua INCLUSIVE 1
  Hello, world!
  {
  	["HELLO-IPROF-LIMITS"] = {
  		calls = 1,
  		lua = 7.4664014391601e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 8.6250365711749e-06,
  				wall = 8.6250365711749e-06
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 8.2789920270443e-06,
  				wall = 8.2789920270443e-06
  			},
  			["function @hello-iprof-limits.lua:20"] = {
  				calls = 1,
  				lua = 4.3603999074548e-05,
  				wall = 4.3603999074548e-05
  			},
  			["function @hello-iprof-limits.lua:25"] = {
  				calls = 1,
  				lua = 5.8030127547681e-06,
  				wall = 5.8030127547681e-06
  			}
  		},
  		wall = 7.4664014391601e-05
  	}
  }

.. code::

  $ ujit -joff hello-iprof-limits.lua EXCLUSIVE 1
  Hello, world!
  {
  	["HELLO-IPROF-LIMITS"] = {
  		calls = 1,
  		lua = 1.579598756507e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 1.6681966371834e-05,
  				wall = 1.6681966371834e-05
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.3125012628734e-05,
  				wall = 1.3125012628734e-05
  			},
  			["function @hello-iprof-limits.lua:20"] = {
  				calls = 1,
  				lua = 0.00010146002750844,
  				wall = 0.00010146002750844
  			},
  			["function @hello-iprof-limits.lua:25"] = {
  				calls = 1,
  				lua = 6.9909729063511e-06,
  				wall = 6.9909729063511e-06
  			}
  		},
  		wall = 1.579598756507e-05
  	}
  }

Now, you see only the first-level callees of ``q`` function. Much more pretty
report for your eyes, isn't it?

.. note::

  OK let's take another look into the second report. Mention the fact that
  though mode is exclusive all stats for callees' of ``qq`` defined at
  ``hello-iprof-limits.lua:20`` are aggregated under its label as if all
  callees bodies are placed directly within ``qq``.

Another one ``limit`` parameter benefit is as follows: there are no internal
limits in platform for profiler memory usage, only system ones. Thereby proper
usage of this parameter also allows to reduce memory overhead while profiling.

Lua stack unwinding while profiling
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This section is proposed to shed some light on exception handling while
profiling. Profiling process doesn't spawn a "safe" scope similar to one
``pcall`` does hence this responsibility is shifted to Lua developers.

Let's see the following snippet calling a wrapped function in a safe scope.

.. code::

  local inspect = require 'inspect'

  function ujit.iprof.profile(pfunction, pcallback, name, mode, level)
    return function(...)
      local s, e = ujit.iprof.start(name, mode, level)
      local t = { pfunction(...) }
      if s then pcallback(ujit.iprof.stop()) else pcb(nil, e) end
      return table.unpack(t)
    end
  end

  local function dead(t, e)
    assert(false, 'This message is not being printed')
    print(inspect(t, { indent = '\t' }))
  end

  local function croak(message) error(message) end

  local mode = ujit.iprof[arg[1] or 'PLAIN']

  local die = ujit.iprof.profile(function(iter)
    local __ = 0
    for _ = 1, iter do
      __ = __ + _
      if _ == iter then
        croak('Hello, world!')
      end
    end
  end, dead, 'SUB-NOT-BEING-PROFILED', ujit.iprof.PLAIN)

  local ok, err = pcall(die, tonumber(arg[2] or 42))
  if not ok then print(err) end

It throws an error on the last loop iteration and this error unwinds Lua stack
up to the most outer frame related to the main chunk. Thus you don't see a
report produced by profiler in the output below.

.. code::

  $ ujit -joff hello-iprof-die.lua INCLUSIVE
  hello-iprof-die.lua:17: Hello, world!

Now let's change a bit our example and move a safe scope into the origin
function.

.. code::

  local inspect = require 'inspect'

  function ujit.iprof.profile(pfunction, pcallback, name, mode, level)
    return function(...)
      local s, e = ujit.iprof.start(name, mode, level)
      local t = { pfunction(...) }
      if s then pcallback(ujit.iprof.stop()) else pcb(nil, e) end
      return table.unpack(t)
    end
  end

  local function stats(t, e)
    if not t then error(e) end
    print(inspect(t, { indent = '\t' }))
  end

  local function croak(message) error(message) end

  local mode = ujit.iprof[arg[1] or 'PLAIN']

  local unwind = ujit.iprof.profile(function(iter)
    local __ = 0
    for _ = 1, iter do
      __ = __ + _
      if _ == iter then
        local ok, err = pcall(croak, 'Hello, world!')
        if not ok then print(err) end
      end
    end
  end, stats, 'HELLO-IPROF-UNWIND', mode)

  unwind(tonumber(arg[2] or 42))

This time we see the report but mention the fact that timings are misrepresented
considering Lua stack unwinding.

.. code::

  $ ujit -joff hello-iprof-unwind.lua INCLUSIVE
  hello-iprof-unwind.lua:17: Hello, world!
  {
  	["HELLO-IPROF-UNWIND"] = {
  		calls = 1,
  		lua = 0.00025242200354114,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 8.8699744082987e-06,
  				wall = 8.8699744082987e-06
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.317000715062e-05,
  				wall = 1.317000715062e-05
  			},
  			["function @hello-iprof-unwind.lua:21"] = {
  				calls = 1,
  				lua = 0.0002179789589718,
  				wall = 0.0002179789589718
  			}
  		},
  		wall = 0.00025242200354114
  	}
  }

Coroutine profiling
^^^^^^^^^^^^^^^^^^^

Now let's puzzle out the difference between lua and wall time values in report.
The code below create a new child coroutine to be profiled with a single yield
within it. After the child coroutine yields, the main coroutine sleeps for a few
seconds, prints a message and resumes the child coroutine execution.

.. code::

  local inspect = require 'inspect'

  local ffi = require 'ffi'
  assert(ffi, 'Please ensure that you have an analogue for sleep(3)')
  ffi.cdef('unsigned int sleep(unsigned int seconds);')
  local sleep = ffi.C.sleep;

  function ujit.iprof.profile(pfunction, pcallback, label, mode, limit)
    return function(...)
       local s, e = ujit.iprof.start(label, mode, limit)
       local t = { pfunction(...) }
       if s then pcallback(ujit.iprof.stop()) else pcallback(nil, e) end
       return table.unpack(t)
    end
  end

  local function stats(t, e)
    if not t then error(e) end
    print(inspect(t, { indent = '\t' }))
  end

  function qr(seconds) coroutine.yield(seconds) end
  function qx(seconds) qr(seconds) end
  function qw(seconds) qx(seconds) end
  function qq(seconds) qw(seconds) end

  local mode = ujit.iprof[arg[1] or 'PLAIN']
  local sleep = tonumber(arg[2] or 16)
  local limit = tonumber(arg[3] or 9)

  local coro = coroutine.create(ujit.iprof.profile(function(seconds)
    return qq(seconds)
  end, stats, 'HELLO-IPROF-CORO', mode, limit))

  local ok, result = coroutine.resume(coro, sleep)
  if not ok then error(result) end

  -- Some function that sleeps for `result` seconds
  sleep(result)
  print('Hello, world!')

  ok, result = coroutine.resume(coro)

  assert(ok and coroutine.status(coro) == 'dead', 'Unexpected behaviour')

The difference between the time the child coroutine being executed and the
profiling time equals exactly the time between its yield and resume.

.. code::

  $ ujit -joff hello-iprof-coro.lua
  Hello, world!
  {
  	["HELLO-IPROF-CORO"] = {
  		calls = 1,
  		lua = 1.7445010598749e-05,
  		wall = 16.000266182993
  	}
  }

As you can see the main contribution in this difference is produced by the sleep
the main coroutine does. Let's decrease the time to sleep and look at the report
for inclusive mode.

.. code::

  $ ujit -joff hello-iprof-coro.lua INCLUSIVE 5
  Hello, world!
  {
  	["HELLO-IPROF-CORO"] = {
  		calls = 1,
  		lua = 7.5089978054166e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 8.7990192696452e-06,
  				wall = 8.7990192696452e-06
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.8060964066535e-05,
  				wall = 1.8060964066535e-05
  			},
  			["function @hello-iprof-coro.lua:23"] = {
  				calls = 1,
  				lua = 3.6311976145953e-05,
  				subs = {
  					["function @hello-iprof-coro.lua:22"] = {
  						calls = 1,
  						lua = 2.7927977498621e-05,
  						subs = {
  							["function @hello-iprof-coro.lua:21"] = {
  								calls = 1,
  								lua = 2.1678977645934e-05,
  								subs = {
  									["function @hello-iprof-coro.lua:20"] = {
  										calls = 1,
  										lua = 1.5911005903035e-05,
  										subs = {
  											["builtin #34"] = {
  												calls = 1,
  												lua = 8.3139748312533e-06,
  												wall = 5.0002951360075
  											}
  										},
  										wall = 5.0003027330386
  									}
  								},
  								wall = 5.0003085010103
  							}
  						},
  						wall = 5.0003147500101
  					}
  				},
  				wall = 5.0003231340088
  			},
  			["function @hello-iprof-coro.lua:29"] = {
  				calls = 1,
  				lua = 5.3319963626564e-06,
  				wall = 5.3319963626564e-06
  			}
  		},
  		wall = 5.0003619120107
  	}
  }

Mention the time attributed to ``builtin #34`` -- it is a ``coroutine.yield``
call in the child coroutine, that lasts for a jiffy in Lua interpreter and spent
5 seconds according to the wall clock. Let's limit the profiling depth and see
how stats change.

.. code::

  $ ujit -joff hello-iprof-coro.lua INCLUSIVE 5 2
  Hello, world!
  {
  	["HELLO-IPROF-CORO"] = {
  		calls = 1,
  		lua = 6.4426043536514e-05,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 8.9500099420547e-06,
  				wall = 8.9500099420547e-06
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.9173021428287e-05,
  				wall = 1.9173021428287e-05
  			},
  			["function @hello-iprof-coro.lua:23"] = {
  				calls = 1,
  				lua = 2.3994012735784e-05,
  				subs = {
  					["function @hello-iprof-coro.lua:22"] = {
  						calls = 1,
  						lua = 1.2719014193863e-05,
  						wall = 5.0002876989893
  					}
  				},
  				wall = 5.0002989739878
  			},
  			["function @hello-iprof-coro.lua:29"] = {
  				calls = 1,
  				lua = 5.6329881772399e-06,
  				wall = 5.6329881772399e-06
  			}
  		},
  		wall = 5.0003394060186
  	}
  }

All time the child coroutine waits to resume its execution is now attributed to
the most inner function containing a ``coroutine.yield`` call.

Please mention, that inclusive mode accumulates the time coroutine waits to all
callers. If we change mode to exclusive one the report differs.

.. code::

  $ ujit -joff hello-iprof-coro.lua EXCLUSIVE 5
  Hello, world!
  {
  	["HELLO-IPROF-CORO"] = {
  		calls = 1,
  		lua = 6.9399829953909e-06,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 1.2408010661602e-05,
  				wall = 1.2408010661602e-05
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.490896102041e-05,
  				wall = 1.490896102041e-05
  			},
  			["function @hello-iprof-coro.lua:23"] = {
  				calls = 1,
  				lua = 9.115959983319e-06,
  				subs = {
  					["function @hello-iprof-coro.lua:22"] = {
  						calls = 1,
  						lua = 6.5400381572545e-06,
  						subs = {
  							["function @hello-iprof-coro.lua:21"] = {
  								calls = 1,
  								lua = 6.0539459809661e-06,
  								subs = {
  									["function @hello-iprof-coro.lua:20"] = {
  										calls = 1,
  										lua = 7.9820165410638e-06,
  										subs = {
  											["builtin #34"] = {
  												calls = 1,
  												lua = 9.3470443971455e-06,
  												wall = 5.0001770540257
  											}
  										},
  										wall = 7.9820165410638e-06
  									}
  								},
  								wall = 6.0539459809661e-06
  							}
  						},
  						wall = 6.5400381572545e-06
  					}
  				},
  				wall = 9.115959983319e-06
  			},
  			["function @hello-iprof-coro.lua:29"] = {
  				calls = 1,
  				lua = 5.7680299505591e-06,
  				wall = 5.7680299505591e-06
  			}
  		},
  		wall = 6.9399829953909e-06
  	}
  }

Now the time coroutine waits its execution is attributed only to the yielding
functions or its most inner caller if the limit depth covers yielding call in
report.

.. code::

  $ ujit -joff hello-iprof-coro.lua EXCLUSIVE 5 2
  Hello, world!
  {
  	["HELLO-IPROF-CORO"] = {
  		calls = 1,
  		lua = 8.0890022218227e-06,
  		subs = {
  			["builtin #223"] = {
  				calls = 1,
  				lua = 1.1270982213318e-05,
  				wall = 1.1270982213318e-05
  			},
  			["builtin #224"] = {
  				calls = 1,
  				lua = 1.4292018022388e-05,
  				wall = 1.4292018022388e-05
  			},
  			["function @hello-iprof-coro.lua:23"] = {
  				calls = 1,
  				lua = 1.3369950465858e-05,
  				subs = {
  					["function @hello-iprof-coro.lua:22"] = {
  						calls = 1,
  						lua = 1.2389034964144e-05,
  						wall = 5.0002167590428
  					}
  				},
  				wall = 1.3369950465858e-05
  			},
  			["function @hello-iprof-coro.lua:29"] = {
  				calls = 1,
  				lua = 6.5490021370351e-06,
  				wall = 6.5490021370351e-06
  			}
  		},
  		wall = 8.0890022218227e-06
  	}
  }
