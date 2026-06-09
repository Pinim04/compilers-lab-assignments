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

| Pass source file                   | Pass pipeline name | Plugin library            | Test IR file                  | Command                                                                                                     |
| ---------------------------------- | ------------------ | ------------------------- | ----------------------------- | ----------------------------------------------------------------------------------------------------------- |
| `src/assignment1/AlgIdentity.cpp` | `alg-identity`     | `./lib/libAlgIdentity.so` | `../test/test_AlgIdentity.ll` | `opt -S -load-pass-plugin ./lib/libAlgIdentity.so -p alg-identity ../test/test_AlgIdentity.ll`              |
| `src/assignment1/MultiInst.cpp`   | `multi-inst`       | `./lib/libMultiInst.so`   | `../test/test_MultiInst.ll`   | `opt -S -load-pass-plugin ./lib/libMultiInst.so -p multi-inst ../test/test_MultiInst.ll`                    |
| `src/assignment1/StrengthRed.cpp` | `strength-red`     | `./lib/libStrengthRed.so` | `../test/test_StrengthRed.ll` | `opt -S -load-pass-plugin ./lib/libStrengthRed.so -p strength-red ../test/test_StrengthRed.ll`              |
| `src/assignment3/CodeMotion.cpp`  | `code-motion`      | `./lib/libCodeMotion.so`  | `../test/test_CodeMotion.ll`  | `opt -S -load-pass-plugin ./lib/libCodeMotion.so -p "loop-simplify,code-motion" ../test/test_CodeMotion.ll` |

Example:

```bash
opt -S -load-pass-plugin ./lib/libStrengthRed.so -p strength-red ../test/test_StrengthRed.ll
```
