# AXIOM Memory Device Driver

----

**NOTE: This repository is a submodule of axiom-evi project. Please clone the
master project from https://github.com/evidence/axiom-evi**

This repository contains the implementation of the AXIOM Memory Device to handle
virtual to physical memory mapping.

## How to compile

```
# Use the axiom-evi/scripts/Makefile to compile AXIOM memory kernel modules
cd axiom-evi/scripts

# Configure all modules (required only the first time)
make MODE=aarch64 configure

# compile AXIOM memory kernel modules
make allocator-drv
```
