# Part of the CERN MAD test suite

This is an adopted subset of the test suite for the CERN MAD project.

The tests were cloned from the project's dev branch at commit
[f279d382](https://github.com/MethodicalAcceleratorDesign/MAD/commit/f279d382ebc7168775df50ca20ecbc219c29c149).

Only selected generic Lua tests from
[tests/utests](https://github.com/MethodicalAcceleratorDesign/MAD/tree/dev/tests/utests)
were adopted. NB! Adopting the rest of the suite may be difficult because
Lua implementation used by the project offers some non-standard syntax sugar.

Original license information and copyright notices are preserved in
suite/*.lua files.

For uJIT-specific amendments, search for "UJIT:" comments.
