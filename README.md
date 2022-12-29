# njvm
own (toy) java virtual machine

small interpreter/jit for a subset of the Java bytecode

All supported instructions for JIT are listed in the constants.h header \
The interpreter is outdated and does not support all the instructions as of now

LLVM necessary for building

run with:
```
njvm <CLASS-FILE>
```
