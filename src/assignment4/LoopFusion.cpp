#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

struct LoopFusionPass : PassInfoMixin<LoopFusionPass>
{
    bool areAdjacent(Loop* L0, Loop* L1) {
        BranchInst* G0 = L0->getLoopGuardBranch();
        BranchInst* G1 = L1->getLoopGuardBranch();

        if (!G0 && !G1) {
            errs() << "Check adiacenza: entrambi non-guarded\n";
            // entrambi non-guarded: exit(L0) == preheader(L1)
            BasicBlock* ExitL0 = L0->getExitBlock();

            errs() << "Exit block di L0: " << *ExitL0 << "\n";
            if (!ExitL0)
                return false; // uscite multiple

            BasicBlock* HeaderL1 = L1->getLoopPreheader();
            if (!HeaderL1) {
                errs() << "Header block di L1: " << *HeaderL1 << "\n";
                HeaderL1 =
                  L1->getHeader(); // fallback: se non c'è preheader, header fa da preheader
                errs() << "Header block di L1 (fallback): " << *HeaderL1 << "\n";
            }

            errs() << "Header block di L1 (finale): " << *HeaderL1 << "\n";

            return ExitL0 == HeaderL1;
        }

        if (G0 && G1) {
            errs() << "Check adiacenza: entrambi guarded\n";
            // entrambi guarded:bypass del guard di L0 == guard block di L1
            BasicBlock* FalseSucc = G0->getSuccessor(1);
            return FalseSucc == G1->getParent();
        }

        return false;
    }

    bool isCFEquivalent(Loop* L0, Loop* L1, DominatorTree& DT, PostDominatorTree& PDT) {
        BranchInst* G0 = L0->getLoopGuardBranch();
        BranchInst* G1 = L1->getLoopGuardBranch();

        if (!G0 && !G1) {
            BasicBlock* H0 = L0->getHeader();
            BasicBlock* H1 = L1->getHeader();

            // H0 dom H1
            // H1 postdom H0
            return DT.dominates(H0, H1) && PDT.dominates(H1, H0);
        }

        if (G0 && G1) {
            BasicBlock* GB0 = G0->getParent();
            BasicBlock* GB1 = G1->getParent();

            // GB0 dom GB1
            // GB1 postdom GB0
            return DT.dominates(GB0, GB1) && PDT.dominates(GB1, GB0);
        }

        return false;
    }

    bool haveSameTripCount(Loop* L0, Loop* L1, ScalarEvolution& SE) {
        const SCEV* TC0 = SE.getBackedgeTakenCount(L0);
        const SCEV* TC1 = SE.getBackedgeTakenCount(L1);

        // controlla che SCEV trovi effettivamente una soluzione
        if (dyn_cast<SCEVCouldNotCompute>(TC0) || dyn_cast<SCEVCouldNotCompute>(TC1))
            return false;

        return TC0 == TC1;
    }

    bool hasNegativeDistanceDependence(Loop* L0,
                                       Loop* L1,
                                       DependenceInfo& DI,
                                       ScalarEvolution& SE) {

        for (BasicBlock* BB0 : L0->getBlocks()) {
            for (Instruction& I0 : *BB0) {
                if (!I0.mayReadOrWriteMemory())
                    continue;

                for (BasicBlock* BB1 : L1->getBlocks()) {
                    for (Instruction& I1 : *BB1) {
                        if (!I1.mayReadOrWriteMemory())
                            continue;

                        // RAR: mai problematico, skip
                        if (!I0.mayWriteToMemory() && !I1.mayWriteToMemory()) {
                            errs() << "RAR: " << I0 << " <-> " << I1 << "\n";
                            continue;
                        }

                        // Primo filtro: esiste una dipendenza?
                        auto Dep = DI.depends(&I0, &I1, true);
                        if (!Dep) {
                            errs() << "No dependence: " << I0 << " <-> " << I1 << "\n";
                            continue;
                        }

                        // Secondo filtro: calcola la distanza con SCEV
                        Value* Ptr0 = getLoadStorePointerOperand(&I0);
                        Value* Ptr1 = getLoadStorePointerOperand(&I1);
                        if (!Ptr0 || !Ptr1) {
                            errs() << "No pointer operands: " << I0 << " <-> " << I1 << "\n";
                            continue;
                        }

                        const SCEV* S0 = SE.getSCEVAtScope(Ptr0, L0);
                        const SCEV* S1 = SE.getSCEVAtScope(Ptr1, L1);

                        const SCEV* Dist = SE.getMinusSCEV(S0, S1);

                        // controlla che SCEV trovi effettivamente una soluzione
                        if (dyn_cast<SCEVCouldNotCompute>(Dist)) {
                            errs() << "Could not compute distance: " << I0 << " <-> " << I1 << "\n";
                            return true;
                        }

                        // c'è una dipendenza a distanza negativa
                        if (SE.isKnownNegative(Dist)) {
                            errs()
                              << "Negative distance dependence: " << I0 << " <-> " << I1 << "\n";
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    bool checkLoopSimplifyForm(Loop* L) {
        if (L->isLoopSimplifyForm()) {
            errs() << "Loop in forma semplificata\n";
            return true;
        } else {
            errs() << "Loop non in forma semplificata, run loop-simplify\n";
            return false;
        }
    }

    PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM) {
        LoopInfo& LI = AM.getResult<LoopAnalysis>(F);
        DominatorTree& DT = AM.getResult<DominatorTreeAnalysis>(F);
        PostDominatorTree& PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
        DependenceInfo& DI = AM.getResult<DependenceAnalysis>(F);
        ScalarEvolution& SE = AM.getResult<ScalarEvolutionAnalysis>(F);
        errs() << "\nAnalizzo la funzione: " << F.getName();

        if (LI.empty()) {
            errs() << "Nessun loop trovato\n";
        }

        SmallVector<Loop*, 8> innerLoops;

        for (Loop* loop : LI)
            for (Loop* L : post_order(loop))
                // We only handle inner-most loops.
                if (L->isInnermost())
                    innerLoops.push_back(L);

        DenseMap<BasicBlock*, unsigned> BBOrder;

        unsigned idx = 0;
        for (BasicBlock& BB : F)
            BBOrder[&BB] = idx++;

        llvm::sort(innerLoops, [&](Loop* A, Loop* B) {
            return BBOrder[A->getHeader()] < BBOrder[B->getHeader()];
        });

        for (int i = 0; i < (int)innerLoops.size() - 1; i++) {
            Loop* L0 = innerLoops[i];
            Loop* L1 = innerLoops[i + 1];

            errs() << "\nAnalisi Loop " << i << " e " << i + 1 << "\n";

            // controlla che i 2 loop siano in simplify form
            if (!checkLoopSimplifyForm(L0))
                continue;
            if (!checkLoopSimplifyForm(L1)) {
                i++;
                continue;
            }

            if (!areAdjacent(L0, L1)) {
                errs() << "Loop non adiacenti\n";
                continue;
            }

            if (!isCFEquivalent(L0, L1, DT, PDT)) {
                errs() << "Loop non CF-equivalenti\n";
                continue;
            }

            if (!haveSameTripCount(L0, L1, SE)) {
                errs() << "Loop con trip count diverso\n";
                continue;
            }

            if (hasNegativeDistanceDependence(L0, L1, DI, SE)) {
                errs() << "Loop con dipendenza a distanza negativa\n";
                continue;
            }

            errs() << "Loop " << i << " e " << i + 1 << " possono essere fusi!\n";
        }

        return PreservedAnalyses::none();
    }

    static bool isRequired() { return true; }
};

// Registrazione
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return { LLVM_PLUGIN_API_VERSION, "LoopFusionPass", LLVM_VERSION_STRING, [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback([](StringRef Name,
                                                      FunctionPassManager& FPM,
                                                      ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "my-loop-fusion") {
                        FPM.addPass(LoopFusionPass());
                        return true;
                    }
                    return false;
                });
            } };
}