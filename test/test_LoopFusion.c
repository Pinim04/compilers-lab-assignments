#include <stdlib.h>

// Dummy functions to force LLVM to respect boundaries
extern void dummy_call();
extern void dummy_use_call(int v);

// =============================================================================
// CATEGORY 1: SANITY CHECKS (SHOULD FUSE)
// =============================================================================

// 1. Ideal Case: Perfect adjacency, identical bounds, local arrays.
void test_ideal_fusion() {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) a[i] = i;
    for (int j = 0; j < 100; j++) b[j] = a[j];
}

// 2. Ideal Chain: Multiple loops to test the i-- logic in your worklist.
void test_triple_fusion() {
    int a[100], b[100], c[100], d[100];
    for (int i = 0; i < 100; i++) a[i] = i; 
    for (int j = 0; j < 100; j++) b[j] = a[j] + 1; 
    for (int k = 0; k < 100; k++) c[k] = b[k] + 2; 
    for (int l = 0; l < 100; l++) d[l] = c[l] + 3; 
}

// =============================================================================
// CATEGORY 2: SANITY CHECKS (MUST ABORT FUSION)
// =============================================================================

// 3. Not Adjacent: Instruction between loops breaks Exit -> Preheader flow.
void test_abort_not_adjacent() {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) a[i] = i;
    dummy_call(); // Blocks adjacency
    for (int j = 0; j < 100; j++) b[j] = a[j];
}

// 4. Control Flow Not Equivalent: L1 might not execute even if L0 does.
void test_abort_not_cfeq(int cond) {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) a[i] = i;
    if (cond) { // Breaks PDT(H1, H0)
        for (int j = 0; j < 100; j++) b[j] = a[j];
    }
}

// 5. Different Trip Counts: 100 vs 50. SCEV should reject.
void test_abort_diff_trip() {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) a[i] = i;
    for (int j = 0; j < 50; j++) b[j] = a[j];
}

// 6. Unknown Trip Count: Different variables. SCEV cannot prove equality.
void test_abort_unknown_trip(int n, int m) {
    int a[100], b[100];
    for (int i = 0; i < n; i++) a[i] = i;
    for (int j = 0; j < m; j++) b[j] = a[j];
}

// 7. Negative Distance Dependence: Read-After-Write violation.
void test_abort_neg_dep() {
    int a[100];
    for (int i = 0; i < 99; i++) a[i] = i;
    for (int j = 0; j < 99; j++) {
        // L1 reads a[j+1] which, if fused, hasn't been written by L0 yet!
        a[j] = a[j+1] + 1; 
    }
}

// 8. Early Exit: Breaks the "single exiting block" rule.
void test_abort_early_exit(int *data) {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) {
        if (data[i] == -1) break; // Creates multiple exits
        a[i] = i;
    }
    for (int j = 0; j < 100; j++) b[j] = a[j];
}

// 9. Potential Aliasing: Pointers passed as args might point to the same memory.
// Your pass should abort because getUnderlyingObject will differ, or dependence fails.
void test_abort_aliasing(int *a, int *b) {
    for (int i = 0; i < 100; i++) a[i] = i; 
    for (int j = 0; j < 100; j++) b[j] = a[j]; 
}

// =============================================================================
// CATEGORY 3: GUARDED LOOPS & LOOP FORMS (SHOULD FUSE)
// =============================================================================

// 10. Dynamic Guarded Loops: Typical loop-rotate output.
void test_guarded_dynamic(int n) {
    int a[100], b[100];
    // If n > 0, guard passes.
    for (int i = 0; i < n; i++) a[i] = i;
    for (int j = 0; j < n; j++) b[j] = a[j];
}

// 11. Complex Identical Guards:
void test_complex_guards(int n, int m) {
    int a[100], b[100];
    if (n > 0 && m > 5) {
        for (int i = 0; i < n; i++) a[i] = i;
        for (int j = 0; j < n; j++) b[j] = a[j] * 3;
    }
}

// 12. Strict Do-While: No header guards, just latches.
void test_strict_do_while(int n) {
    if (n <= 0) return;
    int a[100], b[100];
    int i = 0, j = 0;
    
    do {
        a[i] = i;
        i++;
    } while (i < n);
    
    do {
        b[j] = a[j];
        j++;
    } while (j < n);
}

// =============================================================================
// CATEGORY 4: NESTED LOOPS (SHOULD FUSE)
// =============================================================================

// 13. Sibling Inner Loops: The outer loop should remain intact, inner loops fuse.
void test_nested_siblings() {
    int a[100][100];
    for (int i = 0; i < 100; i++) {
        // These two should fuse into a single j loop
        for (int j = 0; j < 100; j++) a[i][j] = 0;
        for (int k = 0; k < 100; k++) a[i][k] += (i + k);
    }
}

// =============================================================================
// CATEGORY 5: COMPLEX CONTROL FLOW & PHI NODES (STRESS TESTS)
// =============================================================================

// 15. Internal Control Flow (If/Else): Tests if Latch1 instructions migrate.
// (If this crashes, you need to migrate Latch1's PHI nodes to Latch0).
void test_internal_control_flow(int n) {
    int a[100], b[100];
    for (int i = 0; i < n; i++) {
        if (i % 2 == 0) a[i] = 10;
        else a[i] = 20;
    }
    for (int j = 0; j < n; j++) {
        if (a[j] == 10) b[j] = 1;
        else b[j] = 0;
    }
}

// 16. Preheader Hoisting Simulation: Tests if preheader instructions migrate.
// Variables calculated before L1 but used inside it.
void test_hoisted_preheader(int n) {
    int a[100], b[100];
    for (int i = 0; i < n; i++) {
        a[i] = i;
    }
    // 'multiplier' is loop-invariant for L1. LLVM will put its calculation in L1's Preheader.
    // Fusing must not bypass this calculation.
    int multiplier = n * 5; 
    for (int j = 0; j < n; j++) {
        b[j] = a[j] * multiplier;
    }
}
