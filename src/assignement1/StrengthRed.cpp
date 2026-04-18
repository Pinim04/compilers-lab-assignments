#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// ─────────────────────────────────────────────────────────────────────────────
// Funzione helper: restituisce log2(k) se k è potenza di 2, altrimenti -1
// ─────────────────────────────────────────────────────────────────────────────
static int getLog2IfPowerOf2(ConstantInt* CI) {
    return CI->getValue().exactLogBase2();
}

// ─────────────────────────────────────────────────────────────────────────────
// Logica principale: scorre le istruzioni del BasicBlock
// ─────────────────────────────────────────────────────────────────────────────
bool runOnBasicBlock(BasicBlock& B) {
    bool changed = false;

    for (auto iter = B.begin(); iter != B.end();) {
        Instruction& I = *iter++;

        // ── MOLTIPLICAZIONE ──────────────────────────────────────────────────
        if (I.getOpcode() == Instruction::Mul) {
            Value* op0 = I.getOperand(0);
            Value* op1 = I.getOperand(1);

            ConstantInt* constOp = nullptr;
            Value* varOp = nullptr;

            if (ConstantInt* CI = dyn_cast<ConstantInt>(op0)) {
                constOp = CI;
                varOp = op1;
            } else if (ConstantInt* CI = dyn_cast<ConstantInt>(op1)) {
                constOp = CI;
                varOp = op0;
            }

            if (!constOp)
                continue;

            int log2val = getLog2IfPowerOf2(constOp);

            if (log2val != -1) {
                // k è potenza di 2  →  var << log2(k)
                outs() << "SR Mul (pot2): " << I << "\n";
                Instruction* shl = BinaryOperator::Create(
                  Instruction::Shl,
                  varOp,
                  ConstantInt::get(Type::getInt32Ty(I.getContext()), log2val),
                  "",
                  &I);
                I.replaceAllUsesWith(shl);
                I.eraseFromParent();
                changed = true;

            } else {
                APInt incVal = constOp->getValue() + 1;
                APInt decVal = constOp->getValue() - 1;
                int log2inc = incVal.exactLogBase2();
                int log2dec = decVal.exactLogBase2();

                if (log2inc != -1) {
                    // (k+1) è potenza di 2  →  (var << log2(k+1)) - var
                    outs() << "SR Mul (k+1 pot2): " << I << "\n";
                    Instruction* shl = BinaryOperator::Create(
                      Instruction::Shl,
                      varOp,
                      ConstantInt::get(Type::getInt32Ty(I.getContext()), log2inc),
                      "shl_tmp",
                      &I);
                    Instruction* sub = BinaryOperator::Create(Instruction::Sub, shl, varOp, "", &I);
                    I.replaceAllUsesWith(sub);
                    I.eraseFromParent();
                    changed = true;

                } else if (log2dec != -1) {
                    // (k-1) è potenza di 2  →  (var << log2(k-1)) + var
                    outs() << "SR Mul (k-1 pot2): " << I << "\n";
                    Instruction* shl = BinaryOperator::Create(
                      Instruction::Shl,
                      varOp,
                      ConstantInt::get(Type::getInt32Ty(I.getContext()), log2dec),
                      "shl_tmp",
                      &I);
                    Instruction* add = BinaryOperator::Create(Instruction::Add, shl, varOp, "", &I);
                    I.replaceAllUsesWith(add);
                    I.eraseFromParent();
                    changed = true;
                }
            }

            // ── DIVISIONE ────────────────────────────────────────────────────────
        } else if (I.getOpcode() == Instruction::UDiv || I.getOpcode() == Instruction::SDiv) {

            ConstantInt* CI = dyn_cast<ConstantInt>(I.getOperand(1));
            if (!CI)
                continue;

            int log2val = getLog2IfPowerOf2(CI);
            if (log2val == -1)
                continue;

            outs() << "SR Div (pot2): " << I << "\n";

            // UDiv → LShr (logico),  SDiv → AShr (aritmetico)
            Instruction::BinaryOps shiftOp =
              (I.getOpcode() == Instruction::UDiv) ? Instruction::LShr : Instruction::AShr;
            Instruction* shr =
              BinaryOperator::Create(shiftOp,
                                     I.getOperand(0),
                                     ConstantInt::get(Type::getInt32Ty(I.getContext()), log2val),
                                     "",
                                     &I);
            I.replaceAllUsesWith(shr);
            I.eraseFromParent();
            changed = true;
        }
    }

    return changed;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass
// ─────────────────────────────────────────────────────────────────────────────
namespace {

struct StrengthReduction : PassInfoMixin<StrengthReduction>
{

    PreservedAnalyses run(Function& F, FunctionAnalysisManager&) {
        bool changed = false;
        for (BasicBlock& B : F)
            if (runOnBasicBlock(B))
                changed = true;
        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Registrazione
// ─────────────────────────────────────────────────────────────────────────────
llvm::PassPluginLibraryInfo getStrengthReductionPluginInfo() {
    return { LLVM_PLUGIN_API_VERSION,
             "StrengthReduction",
             LLVM_VERSION_STRING,
             [](PassBuilder& PB) {
                 PB.registerPipelineParsingCallback([](StringRef Name,
                                                       FunctionPassManager& FPM,
                                                       ArrayRef<PassBuilder::PipelineElement>) {
                     if (Name == "strenght-red") {
                         FPM.addPass(StrengthReduction());
                         return true;
                     }
                     return false;
                 });
             } };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getStrengthReductionPluginInfo();
}