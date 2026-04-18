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
    // Main entry point, takes IR unit to run the pass on (&F) and the
    // corresponding pass manager (to be queried if need be)
    // PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
    //     bool changed = false;

    //     for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
    //         BasicBlock& B = *Iter;

    //         for (Instruction& inst : llvm::make_early_inc_range(B)) {
    //             if (BinaryOperator* firstInst = dyn_cast<BinaryOperator>(&inst)) {
    //                 auto Ty = firstInst->getType();

    //                 if (firstInst->getOpcode() == llvm::Instruction::Add ||
    //                     firstInst->getOpcode() == llvm::Instruction::Sub) {
    //                     for (Use& fisrtInstUse : llvm::make_early_inc_range(firstInst->uses())) {
    //                         // Get the instruction that is using your value
    //                         User* user = fisrtInstUse.getUser();

    //                         // Get the specific operand number where your value sits
    //                         unsigned OperandNo = fisrtInstUse.getOperandNo();

    //                         // Only check for BinaryOperator users
    //                         if (auto* sndInst = dyn_cast<BinaryOperator>(user)) {
    //                             outs() << "I am operand number " << OperandNo
    //                                    << " in the instruction: " << *sndInst << "\n";

    //                             if (sndInst->getOpcode() == llvm::Instruction::Add ||
    //                                 sndInst->getOpcode() == llvm::Instruction::Sub) {
    //                                 // get the other operand of the second instruction
    //                                 Value* sndInstOtherOp = sndInst->getOperand(1 - OperandNo);

    //                                 // Is the second inst opposite of the first one? (Add vs Sub)
    //                                 if (sndInst->getOpcode() != firstInst->getOpcode()) {
    //                                     outs() << "I am the opposite of the first instruction"
    //                                            << *sndInstOtherOp << "\n";

    //                                     if (sndInstOtherOp == firstInst->getOperand(0)) {
    //                                         // Replace all uses
    //                                         sndInst->replaceAllUsesWith(firstInst->getOperand(1));
    //                                         sndInst->eraseFromParent();

    //                                         changed = true;
    //                                     }

    //                                     if (sndInstOtherOp == firstInst->getOperand(1)) {
    //                                         // Replace all uses
    //                                         sndInst->replaceAllUsesWith(firstInst->getOperand(0));
    //                                         sndInst->eraseFromParent();

    //                                         changed = true;
    //                                     }
    //                                 }
    //                             }
    //                         }
    //                     }
    //                 }
    //             }
    //         }
    //     }
    //     return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    // }

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

                            // find the operand to remove
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
                        Value* possibleSub = binOp->getOperand(i);
                        Value* theY = binOp->getOperand(1 - i);

                        if (auto* innerSub = dyn_cast<BinaryOperator>(possibleSub)) {
                            if (innerSub->getOpcode() == Instruction::Sub) {

                                // Check: (X - Y) + Y -> Replace with X
                                if (innerSub->getOperand(1) == theY) {
                                    outs() << "Optimizing (X - Y) + Y -> X\n";
                                    binOp->replaceAllUsesWith(innerSub->getOperand(0));
                                    binOp->eraseFromParent();
                                    changed = true;
                                    break; // Break the 'i' loop since we deleted the instruction
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