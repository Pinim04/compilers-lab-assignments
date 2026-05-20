; RUN: opt -load-pass-plugin=./CodeMotion.so -passes="loop-simplify,code-motion" -S < %s | FileCheck %s

; ==============================================================================
; TEST 1: Basic Invariant Hoisting
; ==============================================================================
; CHECK-LABEL: define dso_local noundef i32 @_Z10test_basiciii(
; CHECK:       entry:
; CHECK-NEXT:    mul nsw i32 %0, %1
; CHECK-NEXT:    br label
define dso_local noundef i32 @_Z10test_basiciii(i32 noundef %0, i32 noundef %1, i32 noundef %2) {
entry:
  br label %4

4:
  %.01 = phi i32 [ 0, %entry ], [ %9, %10 ]
  %.0 = phi i32 [ 0, %entry ], [ %11, %10 ]
  %5 = icmp slt i32 %.0, %2
  br i1 %5, label %6, label %12

6:
  %7 = mul nsw i32 %0, %1
  %8 = add nsw i32 %7, %.0
  %9 = add nsw i32 %.01, %8
  br label %10

10:
  %11 = add nsw i32 %.0, 1
  br label %4

12:
  ret i32 %.01
}

; ==============================================================================
; TEST 2: Chained Invariants
; ==============================================================================
; CHECK-LABEL: define dso_local noundef i32 @_Z10test_chainiiii(
; CHECK:       entry:
; CHECK-NEXT:    add nsw i32 %0, %1
; CHECK-NEXT:    mul nsw i32
; CHECK-NEXT:    shl i32
; CHECK-NEXT:    br label
define dso_local noundef i32 @_Z10test_chainiiii(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) {
entry:
  br label %5

5:
  %.01 = phi i32 [ 0, %entry ], [ %12, %13 ]
  %.0 = phi i32 [ 0, %entry ], [ %14, %13 ]
  %6 = icmp slt i32 %.0, %3
  br i1 %6, label %7, label %15

7:
  %8 = add nsw i32 %0, %1
  %9 = mul nsw i32 %8, %2
  %10 = shl i32 %9, 2
  %11 = add nsw i32 %10, %.0
  %12 = add nsw i32 %.01, %11
  br label %13

13:
  %14 = add nsw i32 %.0, 1
  br label %5

15:
  ret i32 %.01
}

; ==============================================================================
; TEST 3: Memory Safety
; ==============================================================================
; CHECK-LABEL: define dso_local noundef i32 @_Z11test_memoryPii(
; CHECK:       entry:
; CHECK-NEXT:    br label
; CHECK:         load i32, ptr %0
define dso_local noundef i32 @_Z11test_memoryPii(ptr noundef %0, i32 noundef %1) {
entry:
  br label %3

3:
  %.01 = phi i32 [ 0, %entry ], [ %8, %9 ]
  %.0 = phi i32 [ 0, %entry ], [ %10, %9 ]
  %4 = icmp slt i32 %.0, %1
  br i1 %4, label %5, label %11

5:
  %6 = load i32, ptr %0, align 4
  %7 = add nsw i32 %6, %.0
  %8 = add nsw i32 %.01, %7
  br label %9

9:
  %10 = add nsw i32 %.0, 1
  br label %3

11:
  ret i32 %.01
}

; ==============================================================================
; TEST 4: Dominance Checks
; ==============================================================================
; CHECK-LABEL: define dso_local noundef i32 @_Z14test_dominanceiiii(
; CHECK:       entry:
; CHECK-NEXT:    icmp sgt
; CHECK-NEXT:    br label
; CHECK:         sdiv i32 %0, %1
define dso_local noundef i32 @_Z14test_dominanceiiii(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) {
entry:
  br label %5

5:
  %.02 = phi i32 [ 0, %entry ], [ %8, %13 ]
  %.01 = phi i32 [ 0, %entry ], [ %.1, %13 ]
  %.0 = phi i32 [ 0, %entry ], [ %14, %13 ]
  %6 = icmp slt i32 %.0, %2
  br i1 %6, label %7, label %15

7:
  %8 = add nsw i32 %.02, %.0
  %9 = icmp sgt i32 %3, 5
  br i1 %9, label %10, label %12

10:
  %11 = sdiv i32 %0, %1
  br label %12

12:
  %.1 = phi i32 [ %11, %10 ], [ %.01, %7 ]
  br label %13

13:
  %14 = add nsw i32 %.0, 1
  br label %5

15:
  %16 = add nsw i32 %.02, %.01
  ret i32 %16
}