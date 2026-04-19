# compilers-lab-assignments

Assignments for the Unimore Compilers course (25/26).

## Build

Create a build folder, configure with CMake, and compile:

```bash
mkdir -p build
cd build

# Standard configuration
cmake ..

# If LLVM is installed in a custom location, use:
# cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ..

make -j"$(nproc)"
```

## Run tests

From the `build` directory, run LLVM `opt` with the pass plugin:

| Pass source file | Pass pipeline name | Plugin library | Test IR file | Command |
|---|---|---|---|---|
| `src/assignement1/AlgIdentity.cpp` | `alg-identity` | `./lib/libAlgIdentity.so` | `../test/test_AlgIdentity.ll` | `opt -S -load-pass-plugin ./lib/libAlgIdentity.so -p alg-identity ../test/test_AlgIdentity.ll` |
| `src/assignement1/MultiInst.cpp` | `multi-inst` | `./lib/libMultiInst.so` | `../test/test_MultiInst.ll` | `opt -S -load-pass-plugin ./lib/libMultiInst.so -p multi-inst ../test/test_MultiInst.ll` |
| `src/assignement1/StrengthRed.cpp` | `strength-red` | `./lib/libStrengthRed.so` | `../test/test_StrengthRed.ll` | `opt -S -load-pass-plugin ./lib/libStrengthRed.so -p strength-red ../test/test_StrengthRed.ll` |

Example:

```bash
opt -S -load-pass-plugin ./lib/libStrengthRed.so -p strength-red ../test/test_StrengthRed.ll
```
