.. _utils-cpuinfo:

utils/cpuinfo
=============

Description
-----------

This module allows to access processor identification and feature information as exposed by CPUID instruction and described in **Intel® 64 and IA-32 Architectures Software Developer's Manual**.

Interface
---------

.. code-block:: c

    #include <utils/cpuinfo.h>
             
Functions
---------

``int cpuinfo_has_cmov()``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Returns non-zero if conditional mov CMOV instruction is supported. Returns 0 otherwise.

``int cpuinfo_has_sse2()``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Returns non-zero if SSE2 is supported. Returns 0 otherwise.

``int cpuinfo_has_sse3()``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Returns non-zero if SSE3 is supported. Returns 0 otherwise.

``int cpuinfo_has_sse4_1()``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Returns non-zero if SSE 4.1 is supported. Returns 0 otherwise.

