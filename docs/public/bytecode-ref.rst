.. _bytecode-ref:

Bytecode Reference
==================

.. contents:: :local:

.. warning::

    This document is under active development. It might and probably will change substantially in the future.


Comparison
----------

ISLT
^^^^
ISGE
^^^^
ISLE
^^^^
ISGT
^^^^
ISEQx
^^^^^
ISEQS
^^^^^
ISEQN
^^^^^
ISEQP
^^^^^
ISEQV
^^^^^
ISNEx
^^^^^
ISNES
^^^^^
ISNEN
^^^^^
ISNEP
^^^^^
ISNEV
^^^^^

Unary Test and Copy
-------------------

ISTC
^^^^
ISFC
^^^^
IST
^^^
ISF
^^^

Unary ops
---------

MOV
^^^
NOT
^^^
UNM
^^^
LEN
^^^

Binary ops
----------

ADD
^^^
SUB
^^^
MUL
^^^
DIV
^^^
MOD
^^^
POW
^^^
CAT
^^^

Constant ops
------------

KSTR
^^^^
KCDATA
^^^^^^
KSHORT
^^^^^^
KNUM
^^^^
KPRI
^^^^
KNIL
^^^^

Upvalue and Function ops
------------------------

UGET
^^^^
USETx
^^^^^
USETV
^^^^^
USETS
^^^^^
USETN
^^^^^
USETP
^^^^^
UCLO
^^^^
FNEW
^^^^

Table manipulation
------------------

TNEW
^^^^
TDUP
^^^^
GGET
^^^^
GSET
^^^^
TGETx
^^^^^
TGETV
^^^^^
TGETS
^^^^^
TGETB
^^^^^
TSETx
^^^^^
TSETV
^^^^^
TSETS
^^^^^
TSETB
^^^^^
TSETM
^^^^^

Calls and Vararg Handling
-------------------------

CALLM
^^^^^
CALL
^^^^
CALLMT
^^^^^^
CALLT
^^^^^
ITERC
^^^^^
ITERN
^^^^^
VARG
^^^^
ISNEXT
^^^^^^

Returns
-------

RETM
^^^^
RET
^^^
RET0
^^^^
RET1
^^^^

Loops and branches
------------------

FORI
^^^^
JFORI
^^^^^
FORL
^^^^
IFORL
^^^^^
JFORL
^^^^^
ITERL
^^^^^
IITERL
^^^^^^
JITERL
^^^^^^
LOOP
^^^^
ILOOP
^^^^^
JLOOP
^^^^^
JMP
^^^

Function headers
----------------

FUNCF
^^^^^
IFUNCF
^^^^^^
JFUNCF
^^^^^^
FUNCV
^^^^^
IFUNCV
^^^^^^
JFUNCV
^^^^^^
FUNCC
^^^^^
FUNCCW
^^^^^^
FUNC*
^^^^^

Examples
--------

**Example 1**

``$ cat ./test.lua``

.. code::

    a = 5
    b = 7
    if (a > b) then
    print(a)
    else
    print(b)
    end

``$ ./luajit -bl ./test.lua``

.. code::

    -- BYTECODE -- test.lua:0-8
    0001    KSHORT   0   5
    0002    GSET     0   0      ; "a"
    0003    KSHORT   0   7
    0004    GSET     0   1      ; "b"
    0005    GGET     0   0      ; "a"
    0006    GGET     1   1      ; "b"
    0007    ISGE     1   0
    0008    JMP      0 => 0013
    0009    GGET     0   2      ; "print"
    0010    GGET     1   0      ; "a"
    0011    CALL     0   1   2
    0012    JMP      0 => 0016
    0013 => GGET     0   2      ; "print"
    0014    GGET     1   1      ; "b"
    0015    CALL     0   1   2
    0016 => RET0     0   1