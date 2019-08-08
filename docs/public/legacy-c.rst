.. _legacy-c:

Legacy C Coding Style and Guidelines
====================================

.. contents:: :local:

About this document
-------------------

This document is not intended to be the complete coding style. It is not copy-pasted and/or overcomplicated. It also does not answer *any* question about code composition and styling. The main purpose of this document (yet) is to outline numerous anti-patterns and bad styling solutions that are vastly present in the code base, alongside with the rules on how to fix them and avoid in the future. The main goal of this document is to implement best (or at least best known) practices in order to make the code more maintainable (both easier and faster) and to reduce the possibility of introducing well-known issues.

The application of this document is as follows: if you introduce some changes to the code base and see that particular block of code does not comply to the rules below - it is justified to make additional changes introducing such compliance.

Coding style rules (unstructured, unordered)
--------------------------------------------

- **Two spaces** must be used as indent, tab symbols (``'\t'``) are not allowed.
- Code line width must not exceed **84 symbols**.

    - Except function definitions and declarations, where the entire signature is allowed to be spelled on a single line.

- Opening curly brace must be placed at the same line with corresponding expression such as struct/union/function definition, condition etc. (**K&R style**).
- Compound statements must be used inside selection and iterations statements. This means, conditional blocks and loop iterations must be enclosed in curly braces, *even if* they might be treated as expression statements (e.g. one-liners).
- Pointer star must be attached to the function return type declaration and to variable name in all other cases.
- There should be no comma after the last enumeration member, struct field initializer etc.

Coding style rules: To be agreed
--------------------------------

Below are coding style rules that have not yet been transformed to a consistent policy:

- Equality checks in conditional expressions: both natural style (``if (answer == 42) {...}``) and Yoda style (``if (42 == answer) {...}``) are allowed.

Coding guidelines (unstructured, unordered)
-------------------------------------------

- ``const type*`` should be used where appropriate, i.e. where program logic assumes read-only memory.
- Conditional compilation. Quoting `Linux Kernel Coding Style <https://www.kernel.org/doc/html/v4.10/process/coding-style.html>`__: "Prefer to compile out entire functions, rather than portions of functions or portions of expressions. Rather than putting an ``ifdef`` in an expression, factor out part or all of the expression into a separate helper function and apply the conditional to that function*. Functions that assert system's internal state and are supposed to be called in debug mode only, should contain the word ``assert`` in their names".
