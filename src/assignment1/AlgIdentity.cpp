#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// New PM implementation
struct AlgIdentity : PassInfoMixin<AlgIdentity>
{
    // Main entry point, takes IR unit to run the pass on (&F) and the
    // corresponding pass manager (to be queried if need be)
    PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
        bool changed = false;

        for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
            BasicBlock& B = *Iter;

            for (Instruction& inst : llvm::make_early_inc_range(B)) {
                if (BinaryOperator* BinOp = dyn_cast<BinaryOperator>(&inst)) {
                    auto Ty = BinOp->getType();

                    switch (BinOp->getOpcode()) {
                        case Instruction::Add:
                            if (Value* toKeep =
                                  getIdentityOperand(BinOp, ConstantInt::get(Ty, 0))) {

                                // Stampiamo immediatamente cosa stiamo per eliminare
                                outs() << "OTTIMIZZAZIONE: Rimuovo " << inst
                                       << "\nSostituisco con l'operando: ";
                                toKeep->printAsOperand(outs(), false);
                                outs() << "\n";

                                inst.replaceAllUsesWith(toKeep);
                                inst.eraseFromParent();

                                changed = true;
                            }
                            break;
                        case Instruction::Mul:
                            if (Value* toKeep =
                                  getIdentityOperand(BinOp, ConstantInt::get(Ty, 1))) {
                                // Stampiamo immediatamente cosa stiamo per eliminare
                                outs() << "OTTIMIZZAZIONE: Rimuovo " << inst
                                       << "\nSostituisco con l'operando: ";
                                toKeep->printAsOperand(outs(), false);
                                outs() << "\n";

                                inst.replaceAllUsesWith(toKeep);
                                inst.eraseFromParent();

                                changed = true;
                            }
                            break;
                    }
                }
            }
        }
        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

    Value* getIdentityOperand(BinaryOperator* aBinOp, Constant* identity) {
        Value* op0 = aBinOp->getOperand(0);
        Value* op1 = aBinOp->getOperand(1);

        // Logica di ottimizzazione: Identità Algebrica (x + 0 = x)
        // Caso 1: il secondo operando è una costante intera uguale a 0
        if (ConstantInt* C = dyn_cast<ConstantInt>(op1)) {
            if (C == identity) {
                return op0;
            }
        }
        // Caso 2: il primo operando è una costante intera uguale a 0 (proprietà commutativa)
        else if (ConstantInt* C = dyn_cast<ConstantInt>(op0)) {
            if (C == identity) {
                return op1;
            }
        }

        return nullptr;
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
    return { LLVM_PLUGIN_API_VERSION, "AlgIdentity", LLVM_VERSION_STRING, [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback([](StringRef Name,
                                                      FunctionPassManager& FPM,
                                                      ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "alg-identity") {
                        FPM.addPass(AlgIdentity());
                        return true;
                    }
                    return false;
                });
            } };
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize AlgIdentity when added to the pass pipeline on the
// command line, i.e. via '-passes=test-pass'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getTestPassPluginInfo();
}