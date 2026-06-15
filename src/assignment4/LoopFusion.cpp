#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Transforms/Utils/LoopUtils.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>

using namespace llvm;

struct LoopFusionPass : PassInfoMixin<LoopFusionPass>
{
    // Controlla che il blocco contenga solo istruzioni di controllo di flusso
    bool isBlockEmpty(BasicBlock* BB) {
        for (Instruction& I : *BB) {
            if (isa<ICmpInst>(&I) || isa<PHINode>(&I) || I.isTerminator())
                continue;
            return false;
        }
        return true;
    }

    bool areAdjacent(Loop* L0, Loop* L1) {
        BranchInst* G0 = L0->getLoopGuardBranch();
        BranchInst* G1 = L1->getLoopGuardBranch();

        errs() << "Check adiacenza:\n";
        if (G0) {
            errs() << "L0 è guarded, guard block: " << *G0->getParent() << "\n";
        } else {
            errs() << "L0 non è guarded\n";
        }
        if (G1) {
            errs() << "L1 è guarded, guard block: " << *G1->getParent() << "\n";
        } else {
            errs() << "L1 non è guarded\n";
        }

        if (!G0 && !G1) {
            errs() << "Check adiacenza: entrambi non-guarded\n";
            // entrambi non-guarded: exit(L0) == preheader(L1)
            BasicBlock* ExitL0 = L0->getExitBlock();

            if (!ExitL0)
                return false; // uscite multiple

            errs() << "Exit block di L0: " << *ExitL0 << "\n";
            BasicBlock* PreheaderL1 = L1->getLoopPreheader();
            if (!PreheaderL1)
                return false; // fallback

            errs() << "Preheader block di L1: " << *PreheaderL1 << "\n";

            // Se sono lo stesso blocco, basta controllare che sia vuoto
            if (ExitL0 == PreheaderL1) {
                return isBlockEmpty(ExitL0);
            }

            // Se sono blocchi separati, ExitL0 deve fare falltrhug al Preheader di L1
            if (ExitL0->getSingleSuccessor() == PreheaderL1) {
                return isBlockEmpty(ExitL0) && isBlockEmpty(PreheaderL1);
            }

            return false;
        }

        if (G0 && G1) {
            errs() << "Check adiacenza: entrambi guarded\n";
            BasicBlock* GB1 = G1->getParent();

            // prendere il BB di bypass di G0
            BasicBlock* Preheader0 = L0->getLoopPreheader();
            if (!Preheader0)
                return false; // Sicurezza topologica

            BasicBlock* G0Bypass;
            // se esiste un preheader, fai il check con quello
            if (G0->getSuccessor(0) == Preheader0) {
                G0Bypass = G0->getSuccessor(1);
            } else if (G0->getSuccessor(1) == Preheader0) {
                G0Bypass = G0->getSuccessor(0);
            } else {
                // se la guard passa direttamente all'header
                G0Bypass =
                  L0->contains(G0->getSuccessor(0)) ? G0->getSuccessor(1) : G0->getSuccessor(0);
            }

            errs() << "Guard block di L1: " << *GB1 << "\n";
            errs() << "Bypass di G0: " << *G0Bypass << "\n";

            // bypass del guard di L0 == guard block di L1
            if (G0Bypass == GB1) {
                return isBlockEmpty(GB1);
            }

            // bypass passa per un blocco intermedio prima di GB1
            if (G0Bypass->getSingleSuccessor() == GB1) {
                if (!isBlockEmpty(G0Bypass))
                    return false;

                BasicBlock* ExitL0 = L0->getExitBlock();
                if (ExitL0 && ExitL0->getSingleSuccessor() == GB1) {
                    return isBlockEmpty(ExitL0);
                }
                return false;
            }
            return false;
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
                if (!I0.mayReadOrWriteMemory()) {
                    continue;
                }

                for (BasicBlock* BB1 : L1->getBlocks()) {
                    for (Instruction& I1 : *BB1) {
                        if (!I1.mayReadOrWriteMemory()) {
                            continue;
                        }

                        // RAR: mai problematico, skip
                        if (!I0.mayWriteToMemory() && !I1.mayWriteToMemory()) {
                            errs() << "RAR: " << I0 << " <-> " << I1 << "\n";
                            continue;
                        }

                        // controlla se esiste una dipendenza di qualche tipo
                        auto Dep = DI.depends(&I0, &I1, true);
                        if (!Dep) {
                            errs() << "No dependence: " << I0 << " <-> " << I1 << "\n";
                            continue;
                        }

                        // calcola la distanza con SCEV
                        Value* Ptr0 = getLoadStorePointerOperand(&I0);
                        Value* Ptr1 = getLoadStorePointerOperand(&I1);

                        if (!Ptr0 || !Ptr1) {
                            errs() << "No pointer operands: " << I0 << " <-> " << I1 << "\n";
                            continue;
                        }

                        // controlla la definizione originale dei puntatori
                        if (getUnderlyingObject(Ptr0) != getUnderlyingObject(Ptr1)) {
                            errs() << "Puntatori con base diversa: possibile aliasing " << I0
                                   << " <-> " << I1 << "\n";
                            return true;
                        }

                        const SCEV* S0 = SE.getSCEVAtScope(Ptr0, L0);
                        const SCEV* S1 = SE.getSCEVAtScope(Ptr1, L1);

                        // constrolla che sia una SCEV lineare
                        const SCEVAddRecExpr* AR0 = dyn_cast<SCEVAddRecExpr>(S0);
                        const SCEVAddRecExpr* AR1 = dyn_cast<SCEVAddRecExpr>(S1);

                        if (!AR0 || !AR1) {
                            errs() << "Formule non lineari, abortisco fusione per sicurezza.\n";
                            return true;
                        }

                        // controlla che avanzino con lo stesso "passo"
                        if (AR0->getStepRecurrence(SE) != AR1->getStepRecurrence(SE)) {
                            errs() << "Passo diverso tra i due accessi!\n";
                            return true;
                        }

                        // S0: a[i]   -> Start0 = a[0]
                        // S1: a[j+1] -> Start1 = a[1]
                        const SCEV* Start0 = AR0->getStart();
                        const SCEV* Start1 = AR1->getStart();

                        const SCEV* Dist = SE.getMinusSCEV(Start0, Start1);

                        if (dyn_cast<SCEVCouldNotCompute>(Dist)) {
                            errs() << "SCEV non può calcolare la distanza: " << I0 << " <-> " << I1
                                   << "\n";
                            return true;
                        }

                        // se L0 parte PRIMA di L1 nell'array -> dist neg
                        if (SE.isKnownNegative(Dist)) {
                            errs() << "Negative distance dependence trovata: " << I0 << " <-> "
                                   << I1 << "\n";
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    bool checkLoopSimplifyForm(Loop* L) {
        if (L->isLoopSimplifyForm() || !L->getLoopLatch() || !L->getExitingBlock()) {
            errs() << "Loop in forma canonica\n";
            return true;
        } else {
            errs() << "Loop non in forma canonica, run loop-simplify\n";
            return false;
        }
    }

    PHINode* findIV(Loop* L, ScalarEvolution& SE) {
        if (PHINode* IV = L->getCanonicalInductionVariable())
            return IV;
        for (PHINode& PN : L->getHeader()->phis()) {
            const SCEV* S = SE.getSCEV(&PN);
            if (const SCEVAddRecExpr* AR = dyn_cast<SCEVAddRecExpr>(S))
                if (AR->getLoop() == L && AR->isAffine())
                    if (const SCEVConstant* C = dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE)))
                        if (C->getValue()->isOne())
                            return &PN;
        }
        return nullptr;
    }

    // fusione dei loop L0 e L1
    void fuseLoops(Loop* L0, Loop* L1, ScalarEvolution& SE) {
        BasicBlock* H0 = L0->getHeader();
        BasicBlock* Latch0 = L0->getLoopLatch();
        BasicBlock* Exit0 = L0->getExitBlock();
        BasicBlock* Exiting0 = L0->getExitingBlock();

        BasicBlock* H1 = L1->getHeader();
        BasicBlock* Latch1 = L1->getLoopLatch();
        BasicBlock* Exit1 = L1->getExitBlock();
        BasicBlock* Exiting1 = L1->getExitingBlock();

        BranchInst* G0 = L0->getLoopGuardBranch();
        BranchInst* G1 = L1->getLoopGuardBranch();

        errs() << "  fuseLoops: Inizio la fusione...\n";

        // rimpiazzare tutte le occorrenze dell'IV di L1 con quello di L0
        PHINode* IV0 = findIV(L0, SE);
        PHINode* IV1 = findIV(L1, SE);

        if (!IV0 || !IV1) {
            return;
        }

        if (IV0->getType() != IV1->getType()) {
            return;
        }

        IV1->replaceAllUsesWith(IV0);

        // ricerca del body di L1
        BranchInst* H1Br = dyn_cast<BranchInst>(H1->getTerminator());
        BasicBlock* Body1Start = nullptr;
        int num = H1Br ? H1Br->getNumSuccessors() : 0;

        if (num == 0) {
            errs() << "Errore: H1 non ha successori??\n";
            return;
        } else if (num == 1) {
            // Nei loop ruotati o do-while, l'header è il body
            Body1Start = H1;
        } else {
            // cerchiamo il ramo che entra nel loop
            for (unsigned i = 0; i < num; i++) {
                BasicBlock* Succ = H1Br->getSuccessor(i);
                if (L1->contains(Succ)) {
                    Body1Start = Succ;
                    break;
                }
            }
            // Dopo il calcolo di Body1Start nel caso num > 1:
            if (Body1Start == Latch1) {
                Body1Start = H1;
            }
        }

        if (!Body1Start) {
            errs() << "Errore: non ho trovato il Body di L1!\n";
            return;
        }

        // prendi i predecessori, esterni al loop, di H1
        SmallVector<BasicBlock*, 4> ExternalPreds;
        for (BasicBlock* Pred : predecessors(H1)) {
            if (!L1->contains(Pred)) {
                ExternalPreds.push_back(Pred);
            }
        }

        // dummy phi nodes per lasciare il CFG in uno stato valid
        for (BasicBlock* Pred : ExternalPreds) {
            Instruction* Term = Pred->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == H1) {
                    Term->setSuccessor(i, Exit1);
                }
            }
            H1->removePredecessor(Pred);
            for (PHINode& PN : Exit1->phis()) {
                if (PN.getBasicBlockIndex(Pred) < 0) {
                    PN.addIncoming(Constant::getNullValue(PN.getType()), Pred);
                }
            }
        }

        if (Latch1 && Exit1) {
            Instruction* Term = Latch1->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == H1) {
                    Term->setSuccessor(i, Exit1);
                }
            }
            H1->removePredecessor(Latch1);
            for (PHINode& PN : Exit1->phis()) {
                if (PN.getBasicBlockIndex(Latch1) < 0) {
                    PN.addIncoming(Constant::getNullValue(PN.getType()), Latch1);
                }
            }
        }

        // collega i body
        SmallVector<BasicBlock*, 4> PredsOfLatch0(predecessors(Latch0));
        for (BasicBlock* P : PredsOfLatch0) {
            Instruction* Term = P->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == Latch0) {
                    Term->setSuccessor(i, Body1Start);
                }
            }
        }

        // ricollego Latch0
        SmallVector<BasicBlock*, 4> PredsOfLatch1(predecessors(Latch1));
        for (BasicBlock* P : PredsOfLatch1) {
            Instruction* Term = P->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == Latch1) {
                    Term->setSuccessor(i, Latch0);
                }
            }
        }

        // Ricolleghiamo anche le PHI: ogni PHI in Latch0 che aveva un incoming da un vecchio
        // predecessore ora deve avere un incoming da ciascun nuovo predecessore (da Latch1).
        // Per semplicità, assumiamo che tutte le PHI in Latch0 abbiano un incoming da ciascun
        // vecchio predecessore (altrimenti bisognerebbe gestire i casi mancanti).
        for (PHINode& PN : Latch0->phis()) {
            // Per ogni vecchio predecessore di Latch0
            for (BasicBlock* OldPred : PredsOfLatch0) {
                int idx = PN.getBasicBlockIndex(OldPred);
                if (idx < 0)
                    continue;

                // Prendi il valore incoming associato al vecchio predecessore
                Value* IncomingVal = PN.getIncomingValue(idx);

                // Rimuovi il vecchio predecessore dal PHI
                PN.removeIncomingValue(idx, /*DeletePHIIfEmpty=*/false);

                // Aggiungi un entry per ciascun nuovo predecessore (da Latch1)
                for (BasicBlock* NewPred : PredsOfLatch1) {
                    PN.addIncoming(IncomingVal, NewPred);
                }
            }
        }

        // il blocco exiting di L0 va a exit di L1
        if (Exiting0 && Exit0 && Exit1) {
            Instruction* Term = Exiting0->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == Exit0) {
                    Term->setSuccessor(i, Exit1);
                }
            }

            for (PHINode& PN : Exit1->phis()) {
                int idx = PN.getBasicBlockIndex(Exiting1); // Usa chi esce veramente da L1!
                if (idx >= 0) {
                    PN.setIncomingBlock(idx, Exiting0);
                }
            }
        }

        // se esistono delle guardie, aggiustiamo i collegamenti
        if (G0 && G1) {
            BasicBlock* G0Block = G0->getParent();
            BasicBlock* G1Block = G1->getParent();

            BasicBlock* G1FalseDest = nullptr;
            for (unsigned i = 0; i < G1->getNumSuccessors(); i++) {
                if (G1->getSuccessor(i) != L1->getLoopPreheader()) {
                    G1FalseDest = G1->getSuccessor(i);
                    break;
                }
            }

            if (G1FalseDest) {
                for (unsigned i = 0; i < G0->getNumSuccessors(); i++) {
                    if (G0->getSuccessor(i) == G1Block) {
                        G0->setSuccessor(i, G1FalseDest);
                    }
                }

                for (PHINode& PN : G1FalseDest->phis()) {
                    int idx = PN.getBasicBlockIndex(G1Block);
                    if (idx >= 0) {
                        PN.setIncomingBlock(idx, G0Block);
                    }
                }
            }
            errs() << "  fuseLoops: Guardie unite con successo!\n";
        }

        errs() << "  fuseLoops: CFG ricollegato con successo! I blocchi vecchi verranno eliminati "
                  "dalla DCE.\n";
    }

    PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM) {
        LoopInfo& LI = AM.getResult<LoopAnalysis>(F);
        DominatorTree& DT = AM.getResult<DominatorTreeAnalysis>(F);
        PostDominatorTree& PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
        DependenceInfo& DI = AM.getResult<DependenceAnalysis>(F);
        ScalarEvolution& SE = AM.getResult<ScalarEvolutionAnalysis>(F);

        errs() << "\nAnalizzo la funzione: " << F.getName() << "\n";

        if (LI.empty()) {
            errs() << "Nessun loop trovato\n";
            return PreservedAnalyses::all();
        }

        // Costruisce la worklist degli inner loop
        SmallVector<Loop*, 8> innerLoops;
        for (Loop* L : LI.getLoopsInPreorder()) {
            if (L->isInnermost()) {
                innerLoops.push_back(L);
            }
        }

        bool Changed = false;

        // Itera sulle coppie (L0, L1) adiacenti
        // Se la fusione avviene:
        //   - L1 viene rimosso dalla worklist
        //   - i-- permette di ritentare L0 con il nuovo L1
        for (int i = 0; i < (int)innerLoops.size() - 1; i++) {
            Loop* L0 = innerLoops[i];
            Loop* L1 = innerLoops[i + 1];

            errs() << "\nAnalisi Loop " << i << " e " << i + 1 << "\n";

            // Entrambi devono essere in simplified form
            if (!checkLoopSimplifyForm(L0))
                continue;
            if (!checkLoopSimplifyForm(L1)) {
                // L1 non è fusibile con nessuno come candidato
                // destro: saltiamo anche lui come candidato
                // sinistro nella prossima iterazione.
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

            // Richiediamo che entrambi i loop abbiano un'unica uscita (e quindi un unico
            // exiting block). haveSameTripCount potrebbe essere sufficiente per garantire che i
            // loop abbiano lo stesso numero di iterazioni, ma se ci sono uscite multiple (early
            // exits) la fusione diventa molto più complessa e rischiosa, quindi preferiamo
            // richiedere un'unica uscita per semplicità.
            if (!L0->getExitingBlock() || !L1->getExitingBlock()) {
                errs() << "Loop con uscite interne multiple (early exits), fusione abortita\n";
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

            errs() << "Loop " << i << " e " << i + 1 << " → fusione!\n";
            fuseLoops(L0, L1, SE);

            // L1 non esiste più come loop separato:
            // - lo rimuoviamo dalla worklist
            // - aggiorniamo le analisi sul nuovo CFG
            SE.forgetLoop(L0);
            SE.forgetLoop(L1);

            // per rendere pulita la rimozione di L1
            // muovo i blocchi di L1 in L0
            for (BasicBlock* BB : L1->getBlocks()) {
                if (!L0->contains(BB)) {
                    L0->addBasicBlockToLoop(BB, LI);
                }
            }

            if (Loop* Parent = L1->getParentLoop()) {
                // L1 is a nested loop, remove it from its parent
                Parent->removeChildLoop(llvm::find(*Parent, L1));
            } else {
                LI.erase(L1);
            }
            innerLoops.erase(innerLoops.begin() + i + 1);

            // Elimina i blocchi irraggiungibili
            SmallVector<BasicBlock*, 8> DeadBlocks;
            for (BasicBlock& BB : F) {
                if (pred_empty(&BB) && &BB != &F.getEntryBlock()) {
                    DeadBlocks.push_back(&BB);
                }
            }

            // Prima rimuoviamo tutte le referenze ai blocchi
            //-referenze come phi node che hanno come incoming un blocco morto
            //-referenze come terminatori che hanno come successore un blocco morto
            for (BasicBlock* BB : DeadBlocks) {
                BB->dropAllReferences();
            }
            for (BasicBlock* BB : DeadBlocks) {
                BB->eraseFromParent();
            }

            // Ricalcolo dopo la pulizia
            DT.recalculate(F);
            PDT.recalculate(F);

            formLCSSA(*L0, DT, &LI, &SE);

            // Ritentiamo L0 (quello fuso)
            i--;

            Changed = true;
        }

        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
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