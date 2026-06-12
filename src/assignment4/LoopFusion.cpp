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
#include "llvm/Analysis/ValueTracking.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>

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

            if (!ExitL0)
                return false; // uscite multiple

            errs() << "Exit block di L0: " << *ExitL0 << "\n";
            BasicBlock* HeaderL1 = L1->getLoopPreheader();
            if (!HeaderL1) {
                errs() << "Header block di L1: " << *HeaderL1 << "\n";
                HeaderL1 =
                  L1->getHeader(); // fallback: se non c'è preheader, header fa da preheader
                errs() << "Header block di L1 (fallback): " << *HeaderL1 << "\n";
            }

            errs() << "Header block di L1 (finale): " << *HeaderL1 << "\n";

            // Se i blocchi non coincidono, non sono adiacenti
            if (ExitL0 != HeaderL1){ 
                errs() << "Exit di L0 e header di L1 non coincidono\n";
                return false;
            }

            // FIX ERRORE 1: Verifichiamo che non ci siano istruzioni in mezzo!
            for (Instruction &I : *ExitL0) {
                // I nodi PHI sono innocui, servono solo a LLVM
                if (dyn_cast<PHINode>(&I)){
                    continue;
                }

                // L'istruzione di branch finale è necessaria
                if (I.isTerminator()){
                    continue;
                }
                
                // Se troviamo QUALSIASI altra istruzione (es. call, store, add), 
                // c'è del codice in mezzo!
                errs() << "Trovata istruzione in mezzo ai loop: " << I << "\n";
                return false;
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

        if(G0){
            errs() << "Guard block di L0: " << *G0->getParent() << "\n";
        }else {
            errs() << "L0 non guarded\n";
        }
        if(G1){
            errs() << "Guard block di L1: " << *G1->getParent() << "\n";
        }else {
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
                if (!I0.mayReadOrWriteMemory()){
                    continue;
                }

                for (BasicBlock* BB1 : L1->getBlocks()) {
                    for (Instruction& I1 : *BB1) {
                        if (!I1.mayReadOrWriteMemory()){
                            continue;
                        }

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

                        // Questa funzione ignora tutti i calcoli matematici e 
                        // risale fino a trovare l'allocazione originale (la radice, come int a[100]).
                        if (getUnderlyingObject(Ptr0) != getUnderlyingObject(Ptr1)) {
                            continue; // puntano a oggetti diversi, non c'è dipendenza reale
                        }

                        // 3. LA FORMULA: Prendiamo la formula di accesso a memoria di entrambi i puntatori,
                        ///SCEV vede un'equazione chiamata AddRecurrence: {BaseAddress, +, 4}
                        const SCEV* S0 = SE.getSCEVAtScope(Ptr0, L0);
                        const SCEV* S1 = SE.getSCEVAtScope(Ptr1, L1);
                        
                        // Se SCEV non riesce a trovare una formula (es. per accessi dinamici), blocchiamo la fusione per sicurezza
                        //dyn_cast prova a convertire l'oggetto S0 (che è una formula generica) in un SCEVAddRecExpr, ovvero una formula strettamente lineare che avanza di un passo costante.
                        //Se un loop accede alla memoria in modo casuale o non prevedibile (es. a[rand()] o tramite l'uso di un array di indici a[c[i]]), SCEV non genera un AddRecExpr.
                        const SCEVAddRecExpr* AR0 = dyn_cast<SCEVAddRecExpr>(S0);
                        const SCEVAddRecExpr* AR1 = dyn_cast<SCEVAddRecExpr>(S1);
                        
                        if (!AR0 || !AR1) {
                            errs() << "Formule non lineari, abortisco fusione per sicurezza.\n";
                            return true; 
                        }
                        
                        // 4. LO STOREREWRITTEN (SLIDE 23): La magia.
                        // Prendiamo gli operandi (Partenza e Step) dalla formula del Loop 0...
                        //Con Ops(AR0->operands()) stiamo "smontando" l'equazione del Loop 0 per prenderne i mattoncini base (il valore di partenza e l'incremento)
                        //Con SE.getAddRecExpr(Ops, L1, ...) usiamo quei mattoncini per costruire una nuova equazione identica, 
                        //ma dichiariamo a LLVM che vive nel Loop 1 (L1). L'abbiamo letteralmente teletrasportata. Questo è il famoso StoreRewritten.

                        SmallVector<const SCEV*, 2> Ops(AR0->operands());
                        // ... e creiamo una nuova formula piantandola nel Loop 1!
                        const SCEV* StoreRewritten = SE.getAddRecExpr(Ops, L1, AR0->getNoWrapFlags());

                        // 5. LA SOTTRAZIONE: Ora possiamo sottrarli, abitano nello stesso loop!
                        const SCEV* Dist = SE.getMinusSCEV(StoreRewritten, S1);

                        // Se SCEV va in confusione (non dovrebbe più succedere grazie allo StoreRewritten)
                        if (dyn_cast<SCEVCouldNotCompute>(Dist)) {
                            errs() << "Could not compute distance: " << I0 << " <-> " << I1 << "\n";
                            return true; // Blocchiamo la fusione
                        }

                        // 6. IL VERDETTO: È negativo?
                        if (SE.isKnownNegative(Dist)) {
                            errs() << "Negative distance dependence TROVATA: " << I0 << " <-> " << I1 << "\n";
                            return true; // BLOCCHIAMO LA FUSIONE!
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
        BasicBlock* Exiting1 = L1->getExitingBlock();

        BranchInst* G0 = L0->getLoopGuardBranch();
        BranchInst* G1 = L1->getLoopGuardBranch();

        errs() << "  fuseLoops: Inizio la fusione...\n";

        // =================================================================
        // STEP 0: Unifica le variabili di induzione (RAUW) con Fallback
        // =================================================================
        PHINode* IV0 = L0->getCanonicalInductionVariable();
        PHINode* IV1 = L1->getCanonicalInductionVariable();
        
        // Se LLVM non le riconosce in automatico, le peschiamo a mano dall'Header
        if (!IV0 || !IV1) {
            IV0 = dyn_cast<PHINode>(&*H0->begin());
            IV1 = dyn_cast<PHINode>(&*H1->begin());
        }

        if (IV0 && IV1) {
            IV1->replaceAllUsesWith(IV0);
        }

        

        // =================================================================
        // CALCOLO BODY 1: Troviamo il vero inizio del ciclo
        // =================================================================
        BranchInst* H1Br = dyn_cast<BranchInst>(H1->getTerminator());
        BasicBlock* Body1Start = nullptr;
        int num = H1Br ? H1Br->getNumSuccessors() : 0;
        
        if (num == 0) {
            errs() << "Errore: H1 non ha successori!\n";
            return;
        } else if (num == 1) {
            // Nei loop ruotati o do-while, l'Header È il Body
            Body1Start = H1; 
        } else {
            // A -O0, cerchiamo il ramo che entra nel loop
            for(unsigned i = 0; i < num; i++){
                BasicBlock* Succ = H1Br->getSuccessor(i);
                if (L1->contains(Succ)) {
                    Body1Start = Succ;
                    break;  
                }
            }
        }

        // ... (RAUW, calcolo Body1Start e Pulizia DeadPhis rimangono uguali) ...

        // =================================================================
        // ESEGUIRE PRIMA DELLO STEP 1: Taglio Intrusi e Latch Morto
        // =================================================================
        SmallVector<BasicBlock*, 4> ExternalPreds;
        for (BasicBlock* Pred : predecessors(H1)) {
            if (!L1->contains(Pred)) {
                ExternalPreds.push_back(Pred);
            }
        }

        for (BasicBlock* Pred : ExternalPreds) {
            Instruction* Term = Pred->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == H1) {
                    Term->setSuccessor(i, Exit1);
                }
            }
            H1->removePredecessor(Pred); 
            for (PHINode &PN : Exit1->phis()) {
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
            for (PHINode &PN : Exit1->phis()) {
                if (PN.getBasicBlockIndex(Latch1) < 0) {
                    PN.addIncoming(Constant::getNullValue(PN.getType()), Latch1);
                }
            }
        }



        // =================================================================
        // STEP 1: La branch del body di L0 punta al body di L1
        // =================================================================
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
        // STEP 2: Il body di L1 punta al Latch di L0
        // =================================================================
        SmallVector<BasicBlock*, 4> PredsOfLatch1(predecessors(Latch1));
        for (BasicBlock* P : PredsOfLatch1) {
            Instruction* Term = P->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == Latch1) {
                    Term->setSuccessor(i, Latch0);
                }
            }
        }

        // Aggiornamento PHI di sicurezza su Latch0
        for (PHINode &PN : Latch0->phis()) {
            for (BasicBlock* OldPred : PredsOfLatch0) {
                int idx = PN.getBasicBlockIndex(OldPred);
                if (idx >= 0 && !PredsOfLatch1.empty()) {
                    PN.setIncomingBlock(idx, PredsOfLatch1[0]); 
                }
            }
        }

        // =================================================================
        // STEP 3: L'uscita di L0 punta all'uscita finale (Exit1)
        // =================================================================
        if (Exiting0 && Exit0 && Exit1) {
            Instruction* Term = Exiting0->getTerminator();
            for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
                if (Term->getSuccessor(i) == Exit0) {
                    Term->setSuccessor(i, Exit1);
                }
            }
            
            for (PHINode &PN : Exit1->phis()) {
                int idx = PN.getBasicBlockIndex(H1);
                if (idx >= 0) {
                    PN.setIncomingBlock(idx, Exiting0);
                }
            }
        }

        // =================================================================
        // STEP 4: Cablaggio delle Guardie (se presenti)
        // =================================================================
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

                for (PHINode &PN : G1FalseDest->phis()) {
                    int idx = PN.getBasicBlockIndex(G1Block);
                    if (idx >= 0) {
                        PN.setIncomingBlock(idx, G0Block);
                    }
                }
            }
            errs() << "  fuseLoops: Guardie unite con successo!\n";
        }

        errs() << "  fuseLoops: CFG ricollegato con successo! I blocchi vecchi verranno eliminati dal DCE di LLVM.\n";
    }

    PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM) {
        LoopInfo&          LI  = AM.getResult<LoopAnalysis>(F);
        DominatorTree&     DT  = AM.getResult<DominatorTreeAnalysis>(F);
        PostDominatorTree& PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
        DependenceInfo&    DI  = AM.getResult<DependenceAnalysis>(F);
        ScalarEvolution&   SE  = AM.getResult<ScalarEvolutionAnalysis>(F);

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
        return Changed ? PreservedAnalyses::none()
                       : PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }
};

// Registrazione
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return { LLVM_PLUGIN_API_VERSION, "LoopFusionPass", LLVM_VERSION_STRING,
             [](PassBuilder& PB) {
                 PB.registerPipelineParsingCallback(
                     [](StringRef Name, FunctionPassManager& FPM,
                        ArrayRef<PassBuilder::PipelineElement>) {
                         if (Name == "my-loop-fusion") {
                             FPM.addPass(LoopFusionPass());
                             return true;
                         }
                         return false;
                     });
             } };
}