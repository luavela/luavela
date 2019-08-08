.. _sealing-public:

|PROJECT| Sealing Overview
==========================

Purpose
--------

This document describes briefly what is a sealing feature in |PROJECT|, what is it for and what limitations it has.

What is sealing
---------------

The main idea is to mark some objects as undeletable for Garbage Collector.

The problem
------------

Lua has automatic memory management. The developer does not have to care about allocating and freeing memory. Instead, a special subsystem called garbage collector (aka GC) is periodically invoked to scan the entire memory occupied by the Lua program and free unused memory. So the more data it has to check - the slower it works.

Unfortunately, Lua as a language can't create undeletable objects or mark them somehow. So we waste time on each GC cycle to check objects, we don't need to delete.

The solution
-------------

We forcibly mark some objects as sealed.

The implementation
------------------

Sealed object means an object, which we don't need to delete by GC or mark for deletion.

So we use partially manual memory management to mark some objects as sealed. It means that GC will not collect it or other objects inside it.

It recursively goes through all tables, strings, metatables, functions and mark them with a special seal bit. If an object is marked as sealed then all objects inside are marked as sealed too in order to save processing time. All sealed objects will not be removed from memory (or, technically, they will never be processed during GC's sweep phase) until the corresponding worker thread is terminated.

Why recursively? Because if we have a link from an object marked as sealed to some local function, then this local object won't be sealed and GC can consider it as unreachable and will delete on the next garbage collection cycle. As a result, we will refer to a non-existent data.

Important rules
---------------

Sealing is not manageable feature on the sandbox level, it has no Lua API and it is switched on by default.

- All sealed objects should be read-only and unchangeable.
- Do not change sealed object otherwise you will receive an error.
