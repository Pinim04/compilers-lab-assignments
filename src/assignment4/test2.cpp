// ======================================================================
// 1. IL CASO IDEALE (Deve Fondere)
// Due loop identici senza dipendenze tra di loro.
// ======================================================================
void test_ideal(int n) {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) {
        a[i] = i;
    }
    for (int j = 0; j < 100; j++) {
        b[j] = a[j] * 2;
    }
}

// ======================================================================
// 2. DIPENDENZA A DISTANZA POSITIVA / ZERO (Deve Fondere)
// L1 legge ciò che L0 ha scritto nella STESSA iterazione o in una PASSATA.
// ======================================================================
void test_positive_dep() {
    int a[100];
    for (int i = 0; i < 100; i++) {
        a[i] = i; // Scrive a[i]
    }
    for (int j = 0; j < 100; j++) {
        a[j] = a[j] + 5; // Legge a[j] (distanza 0, sicuro da fondere)
    }
}

// ======================================================================
// 3. DIPENDENZA A DISTANZA NEGATIVA (Deve Fallire: Dipendenza)
// L1 legge "dal futuro". Se fondi, corrompi la logica.
// ======================================================================
void test_negative_dep() {
    int a[100];
    for (int i = 0; i < 100; i++) {
        a[i] = i;
    }
    for (int j = 0; j < 100; j++) {
        a[j] = a[j+1] + 1; // ERRORE! a[j+1] non è ancora stato calcolato dal L0!
    }
}

// ======================================================================
// 4. NON ADIACENTI - Istruzioni in mezzo (Deve Fallire: Adiacenza)
// C'è un'istruzione valida tra la fine di L0 e l'inizio di L1.
// ======================================================================
extern void dummy_function();
void test_not_adjacent() {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) {
        a[i] = i;
    }
    
    a[0] = 999; // Istruzione intrusa!
    dummy_function(); // Altra intrusa!

    for (int j = 0; j < 100; j++) {
        b[j] = a[j];
    }
}

// ======================================================================
// 5. TRIP COUNT DIVERSO (Deve Fallire: Trip Count)
// I loop iterano un numero diverso di volte.
// ======================================================================
void test_different_trip_count() {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) {
        a[i] = i;
    }
    for (int j = 0; j < 50; j++) { // Solo 50 iterazioni
        b[j] = a[j];
    }
}

// ======================================================================
// 6. CFG NON EQUIVALENTE (Deve Fallire: CF Equivalent)
// L1 potrebbe non essere eseguito a causa di un branch.
// ======================================================================
void test_not_cf_equivalent(int flag) {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) {
        a[i] = i;
    }
    
    if (flag) { // Rompe la Post-Dominanza!
        for (int j = 0; j < 100; j++) {
            b[j] = a[j];
        }
    }
}

// ======================================================================
// 7. CASO LIMITE: Loop con Break/Uscite multiple (Deve Fallire: Simplify Form)
// Un loop con una via d'uscita anticipata.
// ======================================================================
void test_multiple_exits() {
    int a[100], b[100];
    for (int i = 0; i < 100; i++) {
        a[i] = i;
        if (i == 50) break; // Uscita anomala
    }
    for (int j = 0; j < 100; j++) {
        b[j] = a[j];
    }
}

// ======================================================================
// 8. CASO GUARDED (Deve Fondere se lanciato con loop-simplify)
// N parametrico: il compilatore non sa se n > 0.
// ======================================================================
void test_guarded(int n) {
    int a[100], b[100];
    // Se lanci con loop-simplify, crea una guardia: if (n > 0) entra;
    for (int i = 0; i < n; i++) {
        a[i] = i;
    }
    // Entrambi avranno la stessa identica guardia strutturale
    for (int j = 0; j < n; j++) {
        b[j] = a[j];
    }
}