.. _utils-strscan:

utils/strscan
=============

This module provides functions for converting strings to numbers.

Interface
---------

.. code-block:: c

    #include <utils/strscan.h>

Enumerations
------------

``StrScanFmt``
^^^^^^^^^^^^^^

.. _str-scanfmt:

.. code-block:: c

    /* Describes conversion status and format of the output. */
    typedef enum {
        STRSCAN_ERROR, /* Error during conversion. All other values denote successful conversion. */
        STRSCAN_NUM,   /* Output is double. */
        STRSCAN_IMAG,  /* Output is double, but the input was warked with I/i suffix. */
        STRSCAN_INT,   /* Output is int32_t, stored in lower 4 bytes of the output buffer. */
        STRSCAN_U32,   /* Output is uint32_t, stored in lower 4 bytes of the output buffer. */
        STRSCAN_I64,   /* Output is int64_t. */
        STRSCAN_U64,   /* Output is uint64_t. */
    } StrScanFmt;

Functions
---------

``StrScanFmt strscan_tonumber(const uint8_t *p, double *d, uint32_t opt)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Converts a string pointed to by ``p`` to a number and stores the result in a buffer pointed to by ``d``. Uses flags in ``opt`` to control the conversion. Supported flags are:

    +-----------------------------------+-----------------------------------+
    | Flag                              | Description                       |
    +===================================+===================================+
    | ``STRSCAN_OPT_TONUM``             | Convert to ``double``.            |
    +-----------------------------------+-----------------------------------+
    | ``STRSCAN_OPT_IMAG``              | Convert to ``double``. If the     |
    |                                   | input is marked with the ``I``    |
    |                                   | suffix, ``STRSCAN_IMAG`` will be  |
    |                                   | returned.                         |
    +-----------------------------------+-----------------------------------+
    | ``STRSCAN_OPT_LL``                | Convert to ``long long`` (both    |
    |                                   | signed and unsigned), allowing    |
    |                                   | following suffixes for the input: |
    |                                   | ``LL``, ``ULL``, ``LLU``.         |
    |                                   |                                   |
    |                                   | **CAVEAT.** As the result of      |
    |                                   | conversion is stored in the       |
    |                                   | buffer pointed to by              |
    |                                   | a ``double *`` pointer, it's      |
    |                                   | caller's responsibility to        |
    |                                   | interpret the output properly.    |
    |                                   | Return value should be checked    |
    |                                   | for this purpose.                 |
    +-----------------------------------+-----------------------------------+
    | ``STRSCAN_OPT_C``                 | Convert to ``int`` or ``long``    |
    |                                   | (both signed and unsigned) ,      |
    |                                   | allowing following suffixes for   |
    |                                   | the input: ``U``, ``UL``, ``LU``. |
    |                                   |                                   |
    |                                   | **CAVEAT.** As the result of      |
    |                                   | conversion is stored in the       |
    |                                   | buffer pointed to by a            |
    |                                   | ``double *`` pointer, it's        |
    |                                   | caller's responsibility to        |
    |                                   | interpret the output properly.    |
    |                                   | Return value should be checked    |
    |                                   | for this purpose. If the input    |
    |                                   | was converted to ``int`` (signed  |
    |                                   | or unsigned), the result is       |
    |                                   | stored in the lower 4 bytes of    |
    |                                   | the output buffer.                |
    +-----------------------------------+-----------------------------------+

Returns conversion status of ``StrScanFmt``.

Literals are parsed according to following EBNF definition (**NB!** This is a *draft* version):

.. code::

    literal                    = spaces , [ sign ] , literal-payload , spaces
    literal-payload            = special-literal | numeric-literal
    special-literal            = ( "I" | "i" ) , ( "N" | "n" ) , ( "F" | "f" ) | ( "I" | "i") , ( "N" | "n" ) , ( "F" | "f" ), ( "I" | "i") , ( "N" | "n" ) , ( "I" | "i") , ( "T" | "t" ), ( "Y" | "y" ) | ( "N" | "n" ) , ( "A" | "a" ) , ( "N" | "n" )
    numeric-literal            = integer-literal | floating-point-literal
    integer-literal            = integer-literal-digits [ integer-literal-suffix ]
    integer-literal-digits     = integer-literal-oct | integer-literal-dec | integer-literal-hex
    integer-literal-suffix     = ( "U" | "u" ) | ( "L" | "l" ) | ( "U" | "u" ) , ( "L" | "l" ) | ( "L" | "l" ) , ( "U" | "u" )  | ( "L" | "l" ) , ( "L" | "l" ) | ( "U" | "u" ) , ( "L" | "l" ) , ( "L" | "l" ) | ( "L" | "l" ) , ( "L" | "l" ) , ( "U" | "u" )
    integer-literal-oct        = "0" , { octal-digit }
    integer-literal-dec        = { decimal-digit } , [ exponent-dec ]
    integer-literal-hex        = "0" , "x" , { hexadecimal-digit } , [ exponent-hex ]
    floating-point-literal     = floating-point-literal-dec | floating-point-literal-hex
    floating-point-literal-dec = { decimal-digit } , "." , [ { decimal-digit } ] , [ exponent-dec ]
    floating-point-literal-hex = "0" , "x" , { hexadecimal-digit } , "." , [ { hexadecimal-digit } ] , [ exponent-hex ]
    exponent-dec               = "E" , [sign] , { decimal-digit }
    exponent-hex               = "P" , [sign] , { decimal-digit }
    octal-digit                = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7"
    decimal-digit              = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
    hexadecimal-digit          = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" | ( "A" | "a" ) | ( "B" | "b" ) | ( "C" | "c" ) | ( "D" | "d" ) | ( "E" | "e" ) | ( "F" | "f" )
    spaces                     = "" | { " " | "\t" }
    sign                       = "+" | "-"
