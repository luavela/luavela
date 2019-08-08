.. _spec-unified-layout:

Unified Layout for Test Suites
==============================

.. warning::

    This document is under active development. It might and probably will change substantially in the future.

Introduction
------------

TBD

Top-Level Layout
----------------

All test suites follow into one of following categories:

    -  ``tests/lang``: Implementation-independent suites for general compliance testing;
    -  ``tests/impl``: Implementation-dependent suites for either targeted testing of features provided by the given implementation or cross-validation one implementation with tests from another implementation;
    -  ``tests/perf``: Performance benchmarks.

Layout for a Suite
------------------

Main Layout
^^^^^^^^^^^

+-------------+--------------------------------------------------------+
| Path        | Description                                            |
+=============+========================================================+
| ``$suite_na | Required. CMake file for the suite.                    |
| me/CMakeLis |                                                        |
| ts.txt``    |                                                        |
+-------------+--------------------------------------------------------+
| ``$suite_na | Required. A file that explicitly manifests all files   |
| me/Sources. | from the suite. Should be used for building correct    |
| cmake``     | target dependencies.                                   |
|             |                                                        |
|             | **NB!** Do **not** use globbing in the manifest.       |
|             | *TODO: Explain why*.                                   |
+-------------+--------------------------------------------------------+
| ``$suite_na | Required. Directory that contains the suite per se. If |
| me/suite``  | the suite was fetched from a VCS repository, the copy  |
|             | must be cleaned from all VCS metadata (``.git`` /      |
|             | ``.hg`` / ``.svn`` directories, etc.).                 |
+-------------+--------------------------------------------------------+
| ``$suite_na | Required. A Bash script that actually launches the     |
| me/run_test | suite incorporating all possible tweaks for running    |
| s.sh``      | it. Requirements:                                      |
|             |                                                        |
|             | -  Upon success, must touch an OK file (see below) and |
|             |    ``exit 0``;                                         |
|             | -  Upon failure, must ``exit 1``;                      |
|             | -  Must be runnable manually:                          |
|             |    ``cd $suite_name && ./run_tests.sh``                |
+-------------+--------------------------------------------------------+
| ``$suite_na | Required. This file must include at least:             |
| me/README.m |                                                        |
| d``         | -  Information about the origin of the suite (e.g.     |
|             |    repo address, and the fetched commit);              |
|             | -  All copyright notices;                              |
+-------------+--------------------------------------------------------+
| ``$suite_na | **TO BE IMPLEMENTED LATER.**                           |
| me/diff``   |                                                        |
|             | Ideally, all external suites must be taken as is. If   |
|             | they require patching for some reason, all patches     |
|             | must be kept separately and then applied on the fly.   |
|             |                                                        |
|             | However, for the first release let's patch in place    |
|             | marking all patched places with comments that start    |
|             | with ``UJIT:`` prefix:                                 |
|             |                                                        |
|             | -  ``-- UJIT:`` Lua                                    |
|             | -  ``# UJIT:`` Perl, Python                            |
|             | -  ``/* UJIT: ... */`` C                               |
|             | -  etc.                                                |
+-------------+--------------------------------------------------------+

Environment Variables
"""""""""""""""""""""

Following environment variables must defined and used by the suite launcher. If any variable from the list is not defined at the launch of run_tests.sh, some pre-agreed default must be used.


   ================= ================================================================
   Variable          Description
   ================= ================================================================
   ``SUITE_DIR``     The root directory of the entire suite.

                     Example: ``$SUITE_DIR/suite`` is a path to the tests themselves.
   ``SUITE_OUT_DIR`` Directory for storing artefacts produced by the suite.
   ================= ================================================================

The following environment variables must be set and used when running tests implemented in Lua:

   ================ =====================================================================================================
   Variable         Description
   ================ =====================================================================================================
   ``LUA_IMPL_BIN`` Path to the Lua implementation binary being tested.

                    Note. This variable replaces an earlier introduced ad hoc ``UJIT_BIN`` variable.
   ``LUA_IMPL_OPT`` Raw string of command line options to be passed directly to the binary specified in ``LUA_IMPL_BIN``.
   ================ =====================================================================================================

Dynamically Produced Artefacts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 25 50
   :header-rows: 1

   * - Path
     - Description
   * - ${CMAKE_CURRENT_BINARY_DIR}/suite_bin
     - Optional. Directory where executables or libraries used in a suite are built. Note: ${SUITE_OUT_DIR} shouldn't be used here, because it's removed after each test run and there's no need to rebuilt executables/libraries each time.
   * - ${SUITE_OUT_DIR}/run_tests.ok
     - Required. An "OK file" that should be touch'ed if the suite runs successfully. Should be used in the corresponding CMakeLists.txt for building correct target dependencies.

