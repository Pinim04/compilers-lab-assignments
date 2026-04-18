#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm-19/llvm/IR/Instruction.h>

using namespace llvm;

namespace {

// New PM implementation
struct MultiInst : PassInfoMixin<MultiInst>
{
    PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
        bool changed = false;

        for (BasicBlock& B : F) {
            for (Instruction& inst : llvm::make_early_inc_range(B)) {

                auto* binOp = dyn_cast<BinaryOperator>(&inst);
                if (!binOp)
                    continue;

                // (a + b) - b
                if (binOp->getOpcode() == Instruction::Sub) {
                    Value* leftOp = binOp->getOperand(0);
                    Value* rightOp = binOp->getOperand(1);

                    if (auto* prevInsst = dyn_cast<BinaryOperator>(leftOp)) {
                        if (prevInsst->getOpcode() == Instruction::Add) {

                            // Find the operand to remove and replace the instruction with value
                            // that doesn't cancel out
                            if (prevInsst->getOperand(1) == rightOp) {
                                binOp->replaceAllUsesWith(prevInsst->getOperand(0));
                                binOp->eraseFromParent();
                                changed = true;
                                continue;
                            }
                            if (prevInsst->getOperand(0) == rightOp) {
                                binOp->replaceAllUsesWith(prevInsst->getOperand(1));
                                binOp->eraseFromParent();
                                changed = true;
                                continue;
                            }
                        }
                    }
                }

                // (a - b) + b
                else if (binOp->getOpcode() == Instruction::Add) {
                    // Addition is commutative, so the subtraction could be on the left OR right
                    for (int i = 0; i < 2; ++i) {
                        Value* leftOp = binOp->getOperand(i);
                        Value* rightOp = binOp->getOperand(1 - i);

                        if (auto* innerSub = dyn_cast<BinaryOperator>(leftOp)) {
                            if (innerSub->getOpcode() == Instruction::Sub) {

                                if (innerSub->getOperand(1) == rightOp) {
                                    binOp->replaceAllUsesWith(innerSub->getOperand(0));
                                    binOp->eraseFromParent();
                                    changed = true;
                                    break; // Found the right inst, don't check the other operand
                                }
                            }
                        }
                    }
                }
            }
        }

        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

    // Without isRequired returning true, this pass will be skipped for functions
    // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
    // all functions with optnone.
    static bool isRequired() { return true; }
};
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getTestPassPluginInfo() {
    return { LLVM_PLUGIN_API_VERSION, "MultiInst", LLVM_VERSION_STRING, [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback([](StringRef Name,
                                                      FunctionPassManager& FPM,
                                                      ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "multi-inst") {
                        FPM.addPass(MultiInst());
                        return true;
                    }
                    return false;
                });
            } };
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize MultiInst when added to the pass pipeline on the
// command line, i.e. via '-passes=test-pass'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getTestPassPluginInfo();
}