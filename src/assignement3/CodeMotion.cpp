#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm-19/llvm/IR/Instruction.h>
#include <llvm/IR/Analysis.h>

using namespace llvm;

struct CodeMotionPass : PassInfoMixin<CodeMotionPass>
{
    std::set<Instruction*> Visited;

    // LOOP INVARIANT
    // =========================================================
    //
    // Un'istruzione è loop invariant se tutti i suoi operandi
    // sono costanti, argomenti, definiti fuori dal loop,
    // oppure a loro volta loop invariant (ricorsione).
    // Usiamo Visited per evitare ricorsione infinita sui PHI node.
    bool isOpLoopInvariant(Value* Op, Loop* L) {
        // const
        if (Constant* C = dyn_cast<Constant>(Op)) {
            errs() << "Found loop invariant constant: " << *C << "\n";
            return true;
        }
        if (Argument* A = dyn_cast<Argument>(Op)) {
            errs() << "Found loop invariant argument: " << *A << "\n";
            return true;
        }

        if (Instruction* OpInst = dyn_cast<Instruction>(Op)) {
            // def fuori
            if (!L->contains(OpInst->getParent())) {
                errs() << "Function defined outside loop: " << OpInst->getName() << "\n";
                return true;
            }
            // def dentro: check ricorsivo
            return isInstLoopInvariant(*OpInst, L);
        }
        return false;
    }

    bool isInstLoopInvariant(Instruction& I, Loop* L) {
        if (I.mayReadOrWriteMemory()) {
            return false;
        }

        if (PHINode* Phi = dyn_cast<PHINode>(&I)) {
            return false;
        }

        if (Visited.count(&I))
            return false; // Serve per evitare cicli infiniti
                          // potrebbe esserci un ciclo di dipendenze

        Visited.insert(&I);
        for (auto& Op : I.operands()) {
            if (!isOpLoopInvariant(Op.get(), L)) {
                Visited.erase(&I);
                return false;
            }
        }

        Visited.erase(&I);
        return true;
    }

    // DOMINA TUTTE LE USCITE oppure DEAD ALL'USCITA
    // =========================================================
    //
    // Le uscite del loop sono i BB dentro il loop che hanno
    // almeno un successore fuori dal loop.
    //
    // Se BB domina tutte le uscite --> l'istruzione viene
    // eseguita almeno una volta --> sicuro spostarla.
    bool dominatesAllExits(Instruction* I, Loop* L, DominatorTree& DT) {
        BasicBlock* BB = I->getParent();

        SmallVector<BasicBlock*> ExitingBlocks;
        L->getExitingBlocks(ExitingBlocks); // BB dentro il loop con archi verso fuori

        // si itera su ogni exiting block per assicurarci che BB domini tutte le uscite
        for (BasicBlock* Exiting : ExitingBlocks) {
            if (!DT.dominates(BB, Exiting)) {
                return false;
            }
        }
        return true; // Domina tutti i blocchi uscenti (eventualmente anche se stesso)
    }

    // oppure se la variabile è dead all'uscita --> non viene usata dopo
    // il loop --> non importa se viene eseguita in più --> sicuro.
    bool isDeadAfterLoop(Instruction* I, Loop* L) {
        // Controlla se I viene usata in qualche BB fuori dal loop
        for (User* U : I->users()) {
            if (auto* UseInst = dyn_cast<Instruction>(U)) {
                BasicBlock* UseBB = UseInst->getParent();

                // Se l'uso è fuori dal loop --> la variabile è viva all'uscita
                if (!L->contains(UseBB)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool dominatesExitsOrDeadAfter(Instruction* I, Loop* L, DominatorTree& DT) {
        // Prima prova: domina tutte le uscite
        if (dominatesAllExits(I, L, DT)) {
            errs() << "Domina tutte le uscite\n";
            return true;
        } else {
            errs() << "Non domina tutte le uscite\n";
        }

        // Seconda prova: variabile dead all'uscita del loop
        if (isDeadAfterLoop(I, L)) {
            errs() << "Variabile dead all'uscita\n";
            return true;
        } else {
            errs() << "Variabile viva all'uscita\n";
        }

        return false;
    }

    // DOMINA TUTTI GLI USI NEL LOOP
    // =========================================================
    //
    // Per ogni uso di I dentro il loop, il BB dove è definita I
    // deve dominare il BB dove si trova l'uso.
    // Se non domina --> esiste un cammino verso l'uso che non
    // passa per la definizione --> spostare cambierebbe la semantica.
    bool dominatesAllUsesInLoop(Instruction* I, Loop* L, DominatorTree& DT) {
        for (auto& U : I->uses()) {
            if (auto* UseInst = dyn_cast<Instruction>(U.getUser())) {
                if (L->contains(UseInst->getParent())) {

                    // qui controlliamo direttamente I e U invece dei relativi BB
                    // così che il DT possa gestire i casi particolari come i PHI
                    if (!DT.dominates(I, U)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM) {
        LoopInfo& LI = AM.getResult<LoopAnalysis>(F);
        DominatorTree& DT = AM.getResult<DominatorTreeAnalysis>(F);
        PreservedAnalyses analysisRes = PreservedAnalyses::all();

        errs() << "Analizzo la funzione: " << F.getName() << "\n";

        if (LI.empty()) {
            errs() << "Nessun loop trovato\n";
            return analysisRes;
        }

        for (Loop* L : LI.getLoopsInPreorder()) {
            errs() << "\nAnalisi Loop\n";

            std::vector<Instruction*> Candidates;

            for (BasicBlock* BB : L->blocks()) {
                for (Instruction& I : *BB) {

                    // Inst come branch, return ecc non le consideriamo
                    if (I.isTerminator())
                        continue;

                    errs() << "\nIstruzione: " << I << "\n";

                    // loop invariant
                    Visited.clear();
                    if (!isInstLoopInvariant(I, L)) {
                        errs() << "Non loop invariant\n";
                        continue;
                    }
                    errs() << "Loop invariant: OK\n";

                    // domina uscite oppure dead
                    if (!dominatesExitsOrDeadAfter(&I, L, DT)) {
                        errs() << "Inst non domina tutte le uscite ed è viva fuori dal loop\n";
                        continue;
                    }
                    errs() << "Domina tutte le uscite oppure è dead all'uscita: OK\n";

                    // domina tutti gli usi nel loop
                    if (!dominatesAllUsesInLoop(&I, L, DT)) {
                        errs() << "Non domina tutti gli usi\n";
                        continue;
                    }
                    errs() << "Domina tutti gli usi: OK\n";

                    errs() << "Candidato alla code motion!\n";
                    Candidates.push_back(&I);
                }
            }

            // Sposta i candidati nel preheader
            BasicBlock* Preheader = L->getLoopPreheader();
            if (!Preheader) {
                errs() << "Loop senza preheader, non posso spostare. Run loop-simplify\n";
                continue;
            }
            if (!Candidates.empty()) {
                for (Instruction* I : Candidates) {
                    errs() << "\nSposto nel preheader: " << *I << "\n";
                    I->moveBefore(Preheader->getTerminator());
                }
                analysisRes = PreservedAnalyses::none();
            }
        }

        return analysisRes;
    }
};

// Registrazione
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return { LLVM_PLUGIN_API_VERSION, "CodeMotionPass", LLVM_VERSION_STRING, [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback([](StringRef Name,
                                                      FunctionPassManager& FPM,
                                                      ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "code-motion") {
                        FPM.addPass(CodeMotionPass());
                        return true;
                    }
                    return false;
                });
            } };
}