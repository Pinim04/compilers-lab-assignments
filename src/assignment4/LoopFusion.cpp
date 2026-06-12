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
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>

using namespace llvm;

struct LoopFusionPass : PassInfoMixin<LoopFusionPass>
{
    // Controlla che il blocco contenga solo istruzioni di controllo di flusso
    bool isBlockEmpty(BasicBlock* BB) {
        for (Instruction& I : *BB) {
            if (isa<PHINode>(&I) || I.isTerminator())
                continue;
            return false;
        }
        return true;
    }

    bool areAdjacent(Loop* L0, Loop* L1) {
        BranchInst* G0 = L0->getLoopGuardBranch();
        BranchInst* G1 = L1->getLoopGuardBranch();

        if (!G0 && !G1) {
            errs() << "Check adiacenza: entrambi non-guarded\n";
            // entrambi non-guarded: exit(L0) == preheader(L1)
            BasicBlock* ExitL0 = L0->getExitBlock();

            if (!ExitL0)
                return false; // uscite multiple

            errs() << "Exit block di L0: " << *ExitL0 << "\n";
            BasicBlock* PreheaderL1 = L1->getLoopPreheader();
            if (!PreheaderL1)
                PreheaderL1 = L1->getHeader(); // fallback

            errs() << "Preheader block di L1: " << *PreheaderL1 << "\n";

            // Se sono lo stesso blocco, basta controllare che sia vuoto
            if (ExitL0 == PreheaderL1) {
                return isBlockEmpty(ExitL0);
            }

            // Se sono blocchi separati, ExitL0 deve fare falltrhug al Preheader di L1
            if (ExitL0->getSingleSuccessor() == PreheaderL1) {
                return isBlockEmpty(ExitL0) && isBlockEmpty(PreheaderL1);
            }

            return true;
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

        if (G0) {
            errs() << "Guard block di L0: " << *G0->getParent() << "\n";
        } else {
            errs() << "L0 non guarded\n";
        }
        if (G1) {
            errs() << "Guard block di L1: " << *G1->getParent() << "\n";
        } else {
            errs() << "L1 non guarded\n";
        }

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
                            errs() << "Dipendenza tra array diversi (alias?): " << I0 << " <-> "
                                   << I1 << "\n";
                            return true; // non possiamo essere sicuri che siano array diversi,
                                         // meglio bloccare la fusione per sicurezza
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
        if (L->isLoopSimplifyForm()) {
            errs() << "Loop in forma semplificata\n";
            return true;
        } else {
            errs() << "Loop non in forma semplificata, run loop-simplify\n";
            return false;
        }
    }

    //=========================================================
    // TRASFORMAZIONE
    // Precondizione: tutte e 4 le condizioni di legalità passate
    // =========================================================
    // =========================================================
    // TRASFORMAZIONE (La vera Loop Fusion geometrica)
    // =========================================================
    void fuseLoops(Loop* L0, Loop* L1) {
        BasicBlock* H0 = L0->getHeader();
        BasicBlock* Latch0 = L0->getLoopLatch();
        BasicBlock* Exit0 = L0->getExitBlock();
        BasicBlock* Exiting0 = L0->getExitingBlock();

        BasicBlock* H1 = L1->getHeader();
        BasicBlock* Latch1 = L1->getLoopLatch();
        BasicBlock* Exit1 = L1->getExitBlock();

        BranchInst* G0 = L0->getLoopGuardBranch();
        BranchInst* G1 = L1->getLoopGuardBranch();

        errs() << "  fuseLoops: Inizio la fusione...\n";

        // 0. Unifica le variabili di induzione (RAUW)
        // La funzione replaceAllUsesWith (spesso chiamata RAUW in gergo LLVM) prende letteralmente
        // ogni singola istruzione nel Loop 1 che utilizzava la variabile IV1 (%j) e la sostituisce
        // con la variabile IV0 (%i).
        PHINode* IV0 = L0->getCanonicalInductionVariable();
        PHINode* IV1 = L1->getCanonicalInductionVariable();
        if (IV0 && IV1) {
            IV1->replaceAllUsesWith(IV0);
        }

        // Trova l'inizio del Body del Loop 1
        BranchInst* H1Br = cast<BranchInst>(H1->getTerminator());
        // Questa riga usa l'operatore ternario ? : di C++ per fare un test: "Chiedo a L1 se il
        // successore 0 fa parte dei blocchi contenuti all'interno del loop (L1->contains(...)). Se
        // sì, allora il successore 0 è l'inizio del Body. Se no, per esclusione, l'inizio del Body
        // deve essere il successore 1".
        BasicBlock* Body1Start =
          L1->contains(H1Br->getSuccessor(0)) ? H1Br->getSuccessor(0) : H1Br->getSuccessor(1);

        // =================================================================
        // STEP 1 (Il tuo): La branch del body di L0 punta al body di L1
        // =================================================================
        // Prendi tutti i blocchi che prima passavano la palla al Latch0. Guarda la loro istruzione
        // di terminazione (il Term, che è un salto). Se vedi che questo salto sta puntando al
        // Latch0, staccalo e devialo verso Body1Start (che abbiamo calcolato prima)".
        SmallVector<BasicBlock*, 4> PredsOfLatch0(predecessors(Latch0));
        for (BasicBlock* P : PredsOfLatch0) {
            Instruction* Term = P->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == Latch0) {
                    Term->setSuccessor(i, Body1Start);
                }
            }
        }

        // =================================================================
        // STEP 2 (Il tuo): Il body di L1 punta al Latch di L0
        // =================================================================
        // Nota: usiamo il Latch di L0 perché è lì che avviene l'incremento (i++)
        // e il ritorno a H0. Il Latch di L1 viene abbandonato.
        // Quindi il for cerca l'uscita del Body di L1 e le dice: "Non puntare più al tuo vecchio
        // Latch1. Punta invece al Latch0".
        SmallVector<BasicBlock*, 4> PredsOfLatch1(predecessors(Latch1));
        for (BasicBlock* P : PredsOfLatch1) {
            Instruction* Term = P->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == Latch1) {
                    Term->setSuccessor(i, Latch0);
                }
            }
        }

        // AGGIORNAMENTO PHI (Sicurezza LLVM): Il Latch0 prima riceveva il flusso
        // dal Body0, ora lo riceve dal Body1. Dobbiamo aggiornare i nodi PHI.
        // l codice fa esattamente questo:
        // for (PHINode &PN : Latch0->phis()): Prende ogni nodo PHI nel Latch0.
        // getBasicBlockIndex(OldPred): Cerca in quale "slot" era salvato il vecchio blocco di
        // provenienza (il Body di L0). PN.setIncomingBlock(idx, PredsOfLatch1[0]): Prende quello
        // slot e ci sovrascrive il nuovo blocco di provenienza (la fine del Body di L1). Risultato:
        // Il nodo PHI è stato "aggiornato" alla nuova topologia del grafo.
        for (PHINode& PN : Latch0->phis()) {
            for (BasicBlock* OldPred : PredsOfLatch0) {
                int idx = PN.getBasicBlockIndex(OldPred);
                // PredsOfLatch1[0] è l'ultimo blocco del body di L1.
                if (idx >= 0 && !PredsOfLatch1.empty()) {
                    PN.setIncomingBlock(idx, PredsOfLatch1[0]);
                }
            }
        }

        // =================================================================
        // STEP 3 (Il tuo): All'header di L0 facciamo puntare l'exit di L1
        // =================================================================
        // Questo for scansiona i salti dell'Header e dice: "Se stai cercando di saltare al vecchio
        // spazio intermedio (Exit0),
        // devia il salto direttamente alla vera e unica uscita del programma, ovvero l'uscita di L1
        // (Exit1)".
        if (Exiting0 && Exit0 && Exit1) {
            Instruction* Term = Exiting0->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == Exit0) {
                    Term->setSuccessor(i, Exit1);
                }
            }
            // ...
        }
        // AGGIORNAMENTO PHI (Sicurezza LLVM): Exit1 prima veniva raggiunto da H1.
        // Ora viene raggiunto da H0. Avvisiamo i nodi PHI del cambio di arco!
        for (PHINode& PN : Exit1->phis()) {
            int idx = PN.getBasicBlockIndex(H1);
            if (idx >= 0) {
                PN.setIncomingBlock(idx, H0);
            }
        }

        // =================================================================
        // STEP 4: Cablaggio delle Guardie (Solo se entrambi sono guarded)
        // =================================================================

        if (G0 && G1) {
            BasicBlock* G0Block = G0->getParent();
            BasicBlock* G1Block = G1->getParent();

            // Troviamo dove punta il ramo "False" di G1 (quello che salta il loop 1)
            BasicBlock* G1FalseDest = nullptr;
            for (unsigned i = 0; i < G1->getNumSuccessors(); i++) {
                if (G1->getSuccessor(i) != L1->getLoopPreheader()) {
                    G1FalseDest = G1->getSuccessor(i);
                    break;
                }
            }

            if (G1FalseDest) {
                // Modifiamo G0 in modo che il suo ramo "False" salti direttamente alla fine di G1,
                // scavalcando completamente G1Block.
                for (unsigned i = 0; i < G0->getNumSuccessors(); i++) {
                    if (G0->getSuccessor(i) == G1Block) {
                        G0->setSuccessor(i, G1FalseDest);
                    }
                }

                // AGGIORNAMENTO PHI (Sicurezza LLVM)
                // Il blocco di destinazione finale ora viene raggiunto da G0Block invece che da
                // G1Block
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
                  "dal DCE di LLVM.\n";
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

        // Costruisce la worklist degli inner loop ordinati
        // per posizione nel CFG (BBOrder = ordine IR).
        SmallVector<Loop*, 8> innerLoops;
        for (Loop* TopL : LI)
            for (Loop* L : post_order(TopL))
                if (L->isInnermost())
                    innerLoops.push_back(L);

        DenseMap<BasicBlock*, unsigned> BBOrder;
        unsigned idx = 0;
        for (BasicBlock& BB : F)
            BBOrder[&BB] = idx++;

        llvm::sort(innerLoops, [&](Loop* A, Loop* B) {
            return BBOrder[A->getHeader()] < BBOrder[B->getHeader()];
        });

        bool Changed = false;

        // Itera sulle coppie (L0, L1) adiacenti nella worklist.
        // Se la fusione avviene:
        //   - L1 viene rimosso dalla worklist
        //   - i-- permette di ritentare L0 con il nuovo L1
        // Se un controllo fallisce si passa semplicemente alla
        // coppia successiva con i++.
        for (int i = 0; i < (int)innerLoops.size() - 1; i++) {
            Loop* L0 = innerLoops[i];
            Loop* L1 = innerLoops[i + 1];

            errs() << "\nAnalisi Loop " << i << " e " << i + 1 << "\n";

            // Entrambi devono essere in simplified form —
            // garantisce preheader, latch e exit unici.
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

            if (!haveSameTripCount(L0, L1, SE)) {
                errs() << "Loop con trip count diverso\n";
                continue;
            }

            if (hasNegativeDistanceDependence(L0, L1, DI, SE)) {
                errs() << "Loop con dipendenza a distanza negativa\n";
                continue;
            }

            errs() << "Loop " << i << " e " << i + 1 << " → fusione!\n";
            fuseLoops(L0, L1);

            // L1 non esiste più come loop separato:
            // lo rimuoviamo dalla worklist.
            innerLoops.erase(innerLoops.begin() + i + 1);

            // Ritentiamo L0 (ora loop fuso) con il nuovo i+1.
            i--;

            Changed = true;
        }

        // Se non abbiamo modificato nulla, preserviamo tutto.
        // Altrimenti invalidiamo le analisi dipendenti dal CFG.
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