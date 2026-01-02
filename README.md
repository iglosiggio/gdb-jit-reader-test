# gdb-jit-reader-test

This is an ultra basic implementation of GDB's [JIT Compilation Interface](https://sourceware.org/gdb/current/onlinedocs/gdb.html/JIT-Interface.html#JIT-Interface).
It uses custom debug info instead of creating an ELF file on the spot.

Running `make gdb` will launch a GDB session with our plugin loaded and an
example executable. This executable asks you for a number and will crash
(assuming non-executable stacks) after that amount of iterations. The place
where this crash happens will have a function named attached to it.
