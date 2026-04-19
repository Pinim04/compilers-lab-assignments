#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

int checkShift(ConstantInt* x, uint64_t& ShiftAmt) {
    APInt num = x->getValue();
    if (num.isPowerOf2()) {
        ShiftAmt = num.logBase2();
        return 1;
    } else if ((num + 1).isPowerOf2()) {
        ShiftAmt = (num + 1).logBase2();
        return 2;
    } else if ((num - 1).isPowerOf2()) {
        ShiftAmt = (num - 1).logBase2();
        return 3;
    }
    return 0;
}

bool optimizeMul(BinaryOperator* OldMul,
                 Value* x,
                 uint64_t num,
                 std::vector<Instruction*>& remove,
                 int check) {
    Constant* ShiftAmt = ConstantInt::get(OldMul->getType(), num);
    Instruction* Shl = BinaryOperator::Create(Instruction::Shl, x, ShiftAmt);
    Shl->insertAfter(OldMul);
    Instruction* FinalInst = Shl;
    switch (check) {
        case 1:
            break;

        case 2: {
            Instruction* Sub =
              BinaryOperator::Create(Instruction::Sub, Shl, ConstantInt::get(Shl->getType(), 1));
            Sub->insertAfter(Shl);
            FinalInst = Sub;
            break;
        }
        case 3: {
            Instruction* Add =
              BinaryOperator::Create(Instruction::Add, Shl, ConstantInt::get(Shl->getType(), 1));
            Add->insertAfter(Shl);
            FinalInst = Add;
            break;
        }
        default:
            errs() << "Errore!\n";
            return false;
    }
    OldMul->replaceAllUsesWith(FinalInst);
    remove.push_back(OldMul);
    return true;
}

bool optimizeDiv(BinaryOperator* OldDiv,
                 Value* x,
                 uint64_t num,
                 std::vector<Instruction*>& remove) {
    Constant* Shift = ConstantInt::get(OldDiv->getType(), num);
    Instruction* Shr = BinaryOperator::Create(Instruction::AShr, x, Shift);
    Shr->insertAfter(OldDiv);
    OldDiv->replaceAllUsesWith(Shr);
    remove.push_back(OldDiv);
    return true;
}

struct StrengthRed : PassInfoMixin<StrengthRed>
{
    PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
        bool Changed = false;

        // Lista in cui salveremo le istruzioni da distruggere alla fine
        std::vector<Instruction*> InstToRemove;

        // Doppio ciclo per esplorare ogni istruzione di ogni blocco
        for (BasicBlock& B : F) {
            for (Instruction& Inst : B) {

                // È un'operazione matematica (Add, Moltiplicazione, ecc)?
                if (auto* BinOp = dyn_cast<BinaryOperator>(&Inst)) {
                    Value* Op0 = BinOp->getOperand(0); // Sinistra
                    Value* Op1 = BinOp->getOperand(1); // Destra
                    // Caso A: Trovata moltiplicazione
                    if (BinOp->getOpcode() == Instruction::Mul) {
                        ConstantInt* C0 = dyn_cast<ConstantInt>(Op0);
                        ConstantInt* C1 = dyn_cast<ConstantInt>(Op1);
                        uint64_t shift = 0;
                        if (C0) {
                            int x = checkShift(C0, shift);
                            if (x != 0) {
                                Changed = optimizeMul(BinOp, Op1, shift, InstToRemove, x);
                                continue;
                            }
                        }
                        if (C1) {
                            int x = checkShift(C1, shift);
                            if (x != 0) {
                                Changed = optimizeMul(BinOp, Op0, shift, InstToRemove, x);
                                continue;
                            }
                        }
                    }

                    if (BinOp->getOpcode() == Instruction::SDiv) {
                        ConstantInt* C0 = dyn_cast<ConstantInt>(Op0);
                        ConstantInt* C1 = dyn_cast<ConstantInt>(Op1);
                        uint64_t shift = 0;

                        if (C1) {
                            int x = checkShift(C1, shift);
                            if (x == 1) {
                                Changed = optimizeDiv(BinOp, Op0, shift, InstToRemove);
                                continue;
                            }
                        }
                    }
                }
            }
        }
        // Eliminiamo fisicamente le vecchie istruzioni
        for (Instruction* I : InstToRemove) {
            I->eraseFromParent();
        }

        // Comunichiamo a LLVM se abbiamo modificato il codice o no
        if (Changed) {
            return PreservedAnalyses::none();
        }

        return PreservedAnalyses::all();
    }
    static bool isRequired() { return true; }
};
} // namespace

// --------------------------------------------------------------------------
// New PM Registration
// --------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getStrengthRedPluginInfo() {
    return { LLVM_PLUGIN_API_VERSION, "StrengthRed", LLVM_VERSION_STRING, [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback([](StringRef Name,
                                                      FunctionPassManager& FPM,
                                                      ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "strength-red") {
                        FPM.addPass(StrengthRed());
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
    return getStrengthRedPluginInfo();
}