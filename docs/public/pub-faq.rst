.. _pub-faq:

Frequently Asked Questions
==========================

.. contents:: :local:

What does "Vela" in |PROJECT| mean?
-----------------------------------

It is Vela constellation.

What does |PRJ_INT| mean in the docs and the core code base?
------------------------------------------------------------

This is an internal name of the project in IPONWEB. Unfortunately, we could not simply strip it off prior to the public release, so apologies for possible confusion. |PROJECT| is the preferred public name of the project, |PRJ_INT| is something like an internal "code name".

What is the level of compatibility with Lua 5.1?
------------------------------------------------

The same as for LuaJIT 2.0. But we wish we could make the implementation closer to the PUC-Rio implementation.

Why not LuaJIT 2.1 + LJ_GC64?
-----------------------------

As far as we know, this solution got stabilized by 2016-2017. Alas, we needed something working earlier. Besides, some experiments we ran in 2015 with LuaJIT 2.1 showed performance degradation for our cases.

What about adding support for Lua 5.2+?
---------------------------------------

Unfortunately, we do not have resources to implement it on our own. However, patches are welcome. We definitely do *not* plan to increase the compatibility gap with the PUC-Rio implementation. E.g. dropping support for C API or any initiative like this is not an option for the project.

Why have you killed support for ARM?
------------------------------------

Because we did not have enough resources to provide an adequate level of testing and support for other OSes and platforms than Linux x86-64.

What about re-enabling support for other OSes and other platforms than Linux x86-64?
------------------------------------------------------------------------------------

We doubt that 32-bit platforms will be supported. Regarding other operating systems, we do not have resources to implement it on our own. However, patches are welcome.
