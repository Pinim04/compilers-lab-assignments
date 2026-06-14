; ModuleID = 'loopFusion.ll'
source_filename = "../test/test_LoopFusion.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_ideal_fusion() #0 {
  %1 = alloca [100 x i32], align 16
  %2 = alloca [100 x i32], align 16
  br label %3

3:                                                ; preds = %8, %0
  %.0 = phi i32 [ 0, %0 ], [ %9, %8 ]
  %4 = icmp slt i32 %.0, 100
  br i1 %4, label %5, label %10

5:                                                ; preds = %3
  %6 = sext i32 %.0 to i64
  %7 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %6
  store i32 %.0, ptr %7, align 4
  br label %8

8:                                                ; preds = %5
  %9 = add nsw i32 %.0, 1
  br label %3, !llvm.loop !6

10:                                               ; preds = %3
  br label %11

11:                                               ; preds = %19, %10
  %.01 = phi i32 [ 0, %10 ], [ %20, %19 ]
  %12 = icmp slt i32 %.01, 100
  br i1 %12, label %13, label %21

13:                                               ; preds = %11
  %14 = sext i32 %.01 to i64
  %15 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %14
  %16 = load i32, ptr %15, align 4
  %17 = sext i32 %.01 to i64
  %18 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %17
  store i32 %16, ptr %18, align 4
  br label %19

19:                                               ; preds = %13
  %20 = add nsw i32 %.01, 1
  br label %11, !llvm.loop !8

21:                                               ; preds = %11
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_triple_fusion() #0 {
  %1 = alloca [100 x i32], align 16
  %2 = alloca [100 x i32], align 16
  %3 = alloca [100 x i32], align 16
  %4 = alloca [100 x i32], align 16
  br label %5

5:                                                ; preds = %10, %0
  %.0 = phi i32 [ 0, %0 ], [ %11, %10 ]
  %6 = icmp slt i32 %.0, 100
  br i1 %6, label %7, label %12

7:                                                ; preds = %5
  %8 = sext i32 %.0 to i64
  %9 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %8
  store i32 %.0, ptr %9, align 4
  br label %10

10:                                               ; preds = %7
  %11 = add nsw i32 %.0, 1
  br label %5, !llvm.loop !9

12:                                               ; preds = %5
  br label %13

13:                                               ; preds = %22, %12
  %.01 = phi i32 [ 0, %12 ], [ %23, %22 ]
  %14 = icmp slt i32 %.01, 100
  br i1 %14, label %15, label %24

15:                                               ; preds = %13
  %16 = sext i32 %.01 to i64
  %17 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %16
  %18 = load i32, ptr %17, align 4
  %19 = add nsw i32 %18, 1
  %20 = sext i32 %.01 to i64
  %21 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %20
  store i32 %19, ptr %21, align 4
  br label %22

22:                                               ; preds = %15
  %23 = add nsw i32 %.01, 1
  br label %13, !llvm.loop !10

24:                                               ; preds = %13
  br label %25

25:                                               ; preds = %34, %24
  %.02 = phi i32 [ 0, %24 ], [ %35, %34 ]
  %26 = icmp slt i32 %.02, 100
  br i1 %26, label %27, label %36

27:                                               ; preds = %25
  %28 = sext i32 %.02 to i64
  %29 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %28
  %30 = load i32, ptr %29, align 4
  %31 = add nsw i32 %30, 2
  %32 = sext i32 %.02 to i64
  %33 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %32
  store i32 %31, ptr %33, align 4
  br label %34

34:                                               ; preds = %27
  %35 = add nsw i32 %.02, 1
  br label %25, !llvm.loop !11

36:                                               ; preds = %25
  br label %37

37:                                               ; preds = %46, %36
  %.03 = phi i32 [ 0, %36 ], [ %47, %46 ]
  %38 = icmp slt i32 %.03, 100
  br i1 %38, label %39, label %48

39:                                               ; preds = %37
  %40 = sext i32 %.03 to i64
  %41 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %40
  %42 = load i32, ptr %41, align 4
  %43 = add nsw i32 %42, 3
  %44 = sext i32 %.03 to i64
  %45 = getelementptr inbounds [100 x i32], ptr %4, i64 0, i64 %44
  store i32 %43, ptr %45, align 4
  br label %46

46:                                               ; preds = %39
  %47 = add nsw i32 %.03, 1
  br label %37, !llvm.loop !12

48:                                               ; preds = %37
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_abort_not_adjacent() #0 {
  %1 = alloca [100 x i32], align 16
  %2 = alloca [100 x i32], align 16
  br label %3

3:                                                ; preds = %8, %0
  %.0 = phi i32 [ 0, %0 ], [ %9, %8 ]
  %4 = icmp slt i32 %.0, 100
  br i1 %4, label %5, label %10

5:                                                ; preds = %3
  %6 = sext i32 %.0 to i64
  %7 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %6
  store i32 %.0, ptr %7, align 4
  br label %8

8:                                                ; preds = %5
  %9 = add nsw i32 %.0, 1
  br label %3, !llvm.loop !13

10:                                               ; preds = %3
  call void (...) @dummy_call()
  br label %11

11:                                               ; preds = %19, %10
  %.01 = phi i32 [ 0, %10 ], [ %20, %19 ]
  %12 = icmp slt i32 %.01, 100
  br i1 %12, label %13, label %21

13:                                               ; preds = %11
  %14 = sext i32 %.01 to i64
  %15 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %14
  %16 = load i32, ptr %15, align 4
  %17 = sext i32 %.01 to i64
  %18 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %17
  store i32 %16, ptr %18, align 4
  br label %19

19:                                               ; preds = %13
  %20 = add nsw i32 %.01, 1
  br label %11, !llvm.loop !14

21:                                               ; preds = %11
  ret void
}

declare void @dummy_call(...) #1

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_abort_not_cfeq(i32 noundef %0) #0 {
  %2 = alloca [100 x i32], align 16
  %3 = alloca [100 x i32], align 16
  br label %4

4:                                                ; preds = %9, %1
  %.01 = phi i32 [ 0, %1 ], [ %10, %9 ]
  %5 = icmp slt i32 %.01, 100
  br i1 %5, label %6, label %11

6:                                                ; preds = %4
  %7 = sext i32 %.01 to i64
  %8 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %7
  store i32 %.01, ptr %8, align 4
  br label %9

9:                                                ; preds = %6
  %10 = add nsw i32 %.01, 1
  br label %4, !llvm.loop !15

11:                                               ; preds = %4
  %12 = icmp ne i32 %0, 0
  br i1 %12, label %13, label %25

13:                                               ; preds = %11
  br label %14

14:                                               ; preds = %22, %13
  %.0 = phi i32 [ 0, %13 ], [ %23, %22 ]
  %15 = icmp slt i32 %.0, 100
  br i1 %15, label %16, label %24

16:                                               ; preds = %14
  %17 = sext i32 %.0 to i64
  %18 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %17
  %19 = load i32, ptr %18, align 4
  %20 = sext i32 %.0 to i64
  %21 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %20
  store i32 %19, ptr %21, align 4
  br label %22

22:                                               ; preds = %16
  %23 = add nsw i32 %.0, 1
  br label %14, !llvm.loop !16

24:                                               ; preds = %14
  br label %25

25:                                               ; preds = %24, %11
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_abort_diff_trip() #0 {
  %1 = alloca [100 x i32], align 16
  %2 = alloca [100 x i32], align 16
  br label %3

3:                                                ; preds = %8, %0
  %.0 = phi i32 [ 0, %0 ], [ %9, %8 ]
  %4 = icmp slt i32 %.0, 100
  br i1 %4, label %5, label %10

5:                                                ; preds = %3
  %6 = sext i32 %.0 to i64
  %7 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %6
  store i32 %.0, ptr %7, align 4
  br label %8

8:                                                ; preds = %5
  %9 = add nsw i32 %.0, 1
  br label %3, !llvm.loop !17

10:                                               ; preds = %3
  br label %11

11:                                               ; preds = %19, %10
  %.01 = phi i32 [ 0, %10 ], [ %20, %19 ]
  %12 = icmp slt i32 %.01, 50
  br i1 %12, label %13, label %21

13:                                               ; preds = %11
  %14 = sext i32 %.01 to i64
  %15 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %14
  %16 = load i32, ptr %15, align 4
  %17 = sext i32 %.01 to i64
  %18 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %17
  store i32 %16, ptr %18, align 4
  br label %19

19:                                               ; preds = %13
  %20 = add nsw i32 %.01, 1
  br label %11, !llvm.loop !18

21:                                               ; preds = %11
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_abort_unknown_trip(i32 noundef %0, i32 noundef %1) #0 {
  %3 = alloca [100 x i32], align 16
  %4 = alloca [100 x i32], align 16
  br label %5

5:                                                ; preds = %10, %2
  %.01 = phi i32 [ 0, %2 ], [ %11, %10 ]
  %6 = icmp slt i32 %.01, %0
  br i1 %6, label %7, label %12

7:                                                ; preds = %5
  %8 = sext i32 %.01 to i64
  %9 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %8
  store i32 %.01, ptr %9, align 4
  br label %10

10:                                               ; preds = %7
  %11 = add nsw i32 %.01, 1
  br label %5, !llvm.loop !19

12:                                               ; preds = %5
  br label %13

13:                                               ; preds = %21, %12
  %.0 = phi i32 [ 0, %12 ], [ %22, %21 ]
  %14 = icmp slt i32 %.0, %1
  br i1 %14, label %15, label %23

15:                                               ; preds = %13
  %16 = sext i32 %.0 to i64
  %17 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %16
  %18 = load i32, ptr %17, align 4
  %19 = sext i32 %.0 to i64
  %20 = getelementptr inbounds [100 x i32], ptr %4, i64 0, i64 %19
  store i32 %18, ptr %20, align 4
  br label %21

21:                                               ; preds = %15
  %22 = add nsw i32 %.0, 1
  br label %13, !llvm.loop !20

23:                                               ; preds = %13
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_abort_neg_dep() #0 {
  %1 = alloca [100 x i32], align 16
  br label %2

2:                                                ; preds = %7, %0
  %.0 = phi i32 [ 0, %0 ], [ %8, %7 ]
  %3 = icmp slt i32 %.0, 99
  br i1 %3, label %4, label %9

4:                                                ; preds = %2
  %5 = sext i32 %.0 to i64
  %6 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %5
  store i32 %.0, ptr %6, align 4
  br label %7

7:                                                ; preds = %4
  %8 = add nsw i32 %.0, 1
  br label %2, !llvm.loop !21

9:                                                ; preds = %2
  br label %10

10:                                               ; preds = %20, %9
  %.01 = phi i32 [ 0, %9 ], [ %21, %20 ]
  %11 = icmp slt i32 %.01, 99
  br i1 %11, label %12, label %22

12:                                               ; preds = %10
  %13 = add nsw i32 %.01, 1
  %14 = sext i32 %13 to i64
  %15 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %14
  %16 = load i32, ptr %15, align 4
  %17 = add nsw i32 %16, 1
  %18 = sext i32 %.01 to i64
  %19 = getelementptr inbounds [100 x i32], ptr %1, i64 0, i64 %18
  store i32 %17, ptr %19, align 4
  br label %20

20:                                               ; preds = %12
  %21 = add nsw i32 %.01, 1
  br label %10, !llvm.loop !22

22:                                               ; preds = %10
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_abort_early_exit(ptr noundef %0) #0 {
  %2 = alloca [100 x i32], align 16
  %3 = alloca [100 x i32], align 16
  br label %4

4:                                                ; preds = %15, %1
  %.01 = phi i32 [ 0, %1 ], [ %16, %15 ]
  %5 = icmp slt i32 %.01, 100
  br i1 %5, label %6, label %.loopexit

6:                                                ; preds = %4
  %7 = sext i32 %.01 to i64
  %8 = getelementptr inbounds i32, ptr %0, i64 %7
  %9 = load i32, ptr %8, align 4
  %10 = icmp eq i32 %9, -1
  br i1 %10, label %11, label %12

11:                                               ; preds = %6
  br label %17

12:                                               ; preds = %6
  %13 = sext i32 %.01 to i64
  %14 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %13
  store i32 %.01, ptr %14, align 4
  br label %15

15:                                               ; preds = %12
  %16 = add nsw i32 %.01, 1
  br label %4, !llvm.loop !23

.loopexit:                                        ; preds = %4
  br label %17

17:                                               ; preds = %.loopexit, %11
  br label %18

18:                                               ; preds = %26, %17
  %.0 = phi i32 [ 0, %17 ], [ %27, %26 ]
  %19 = icmp slt i32 %.0, 100
  br i1 %19, label %20, label %28

20:                                               ; preds = %18
  %21 = sext i32 %.0 to i64
  %22 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %21
  %23 = load i32, ptr %22, align 4
  %24 = sext i32 %.0 to i64
  %25 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %24
  store i32 %23, ptr %25, align 4
  br label %26

26:                                               ; preds = %20
  %27 = add nsw i32 %.0, 1
  br label %18, !llvm.loop !24

28:                                               ; preds = %18
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_abort_aliasing(ptr noundef %0, ptr noundef %1) #0 {
  br label %3

3:                                                ; preds = %8, %2
  %.01 = phi i32 [ 0, %2 ], [ %9, %8 ]
  %4 = icmp slt i32 %.01, 100
  br i1 %4, label %5, label %10

5:                                                ; preds = %3
  %6 = sext i32 %.01 to i64
  %7 = getelementptr inbounds i32, ptr %0, i64 %6
  store i32 %.01, ptr %7, align 4
  br label %8

8:                                                ; preds = %5
  %9 = add nsw i32 %.01, 1
  br label %3, !llvm.loop !25

10:                                               ; preds = %3
  br label %11

11:                                               ; preds = %19, %10
  %.0 = phi i32 [ 0, %10 ], [ %20, %19 ]
  %12 = icmp slt i32 %.0, 100
  br i1 %12, label %13, label %21

13:                                               ; preds = %11
  %14 = sext i32 %.0 to i64
  %15 = getelementptr inbounds i32, ptr %0, i64 %14
  %16 = load i32, ptr %15, align 4
  %17 = sext i32 %.0 to i64
  %18 = getelementptr inbounds i32, ptr %1, i64 %17
  store i32 %16, ptr %18, align 4
  br label %19

19:                                               ; preds = %13
  %20 = add nsw i32 %.0, 1
  br label %11, !llvm.loop !26

21:                                               ; preds = %11
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_guarded_dynamic(i32 noundef %0) #0 {
  %2 = alloca [100 x i32], align 16
  %3 = alloca [100 x i32], align 16
  br label %4

4:                                                ; preds = %9, %1
  %.01 = phi i32 [ 0, %1 ], [ %10, %9 ]
  %5 = icmp slt i32 %.01, %0
  br i1 %5, label %6, label %11

6:                                                ; preds = %4
  %7 = sext i32 %.01 to i64
  %8 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %7
  store i32 %.01, ptr %8, align 4
  br label %9

9:                                                ; preds = %6
  %10 = add nsw i32 %.01, 1
  br label %4, !llvm.loop !27

11:                                               ; preds = %4
  br label %12

12:                                               ; preds = %20, %11
  %.0 = phi i32 [ 0, %11 ], [ %21, %20 ]
  %13 = icmp slt i32 %.0, %0
  br i1 %13, label %14, label %22

14:                                               ; preds = %12
  %15 = sext i32 %.0 to i64
  %16 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %15
  %17 = load i32, ptr %16, align 4
  %18 = sext i32 %.0 to i64
  %19 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %18
  store i32 %17, ptr %19, align 4
  br label %20

20:                                               ; preds = %14
  %21 = add nsw i32 %.0, 1
  br label %12, !llvm.loop !28

22:                                               ; preds = %12
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_complex_guards(i32 noundef %0, i32 noundef %1) #0 {
  %3 = alloca [100 x i32], align 16
  %4 = alloca [100 x i32], align 16
  %5 = icmp sgt i32 %0, 0
  br i1 %5, label %6, label %29

6:                                                ; preds = %2
  %7 = icmp sgt i32 %1, 5
  br i1 %7, label %8, label %29

8:                                                ; preds = %6
  br label %9

9:                                                ; preds = %14, %8
  %.01 = phi i32 [ 0, %8 ], [ %15, %14 ]
  %10 = icmp slt i32 %.01, %0
  br i1 %10, label %11, label %16

11:                                               ; preds = %9
  %12 = sext i32 %.01 to i64
  %13 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %12
  store i32 %.01, ptr %13, align 4
  br label %14

14:                                               ; preds = %11
  %15 = add nsw i32 %.01, 1
  br label %9, !llvm.loop !29

16:                                               ; preds = %9
  br label %17

17:                                               ; preds = %26, %16
  %.0 = phi i32 [ 0, %16 ], [ %27, %26 ]
  %18 = icmp slt i32 %.0, %0
  br i1 %18, label %19, label %28

19:                                               ; preds = %17
  %20 = sext i32 %.0 to i64
  %21 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %20
  %22 = load i32, ptr %21, align 4
  %23 = mul nsw i32 %22, 3
  %24 = sext i32 %.0 to i64
  %25 = getelementptr inbounds [100 x i32], ptr %4, i64 0, i64 %24
  store i32 %23, ptr %25, align 4
  br label %26

26:                                               ; preds = %19
  %27 = add nsw i32 %.0, 1
  br label %17, !llvm.loop !30

28:                                               ; preds = %17
  br label %29

29:                                               ; preds = %28, %6, %2
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_strict_do_while(i32 noundef %0) #0 {
  %2 = alloca [100 x i32], align 16
  %3 = alloca [100 x i32], align 16
  %4 = icmp sle i32 %0, 0
  br i1 %4, label %5, label %6

5:                                                ; preds = %1
  br label %23

6:                                                ; preds = %1
  br label %7

7:                                                ; preds = %11, %6
  %.01 = phi i32 [ 0, %6 ], [ %10, %11 ]
  %8 = sext i32 %.01 to i64
  %9 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %8
  store i32 %.01, ptr %9, align 4
  %10 = add nsw i32 %.01, 1
  br label %11

11:                                               ; preds = %7
  %12 = icmp slt i32 %10, %0
  br i1 %12, label %7, label %13, !llvm.loop !31

13:                                               ; preds = %11
  br label %14

14:                                               ; preds = %21, %13
  %.0 = phi i32 [ 0, %13 ], [ %20, %21 ]
  %15 = sext i32 %.0 to i64
  %16 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %15
  %17 = load i32, ptr %16, align 4
  %18 = sext i32 %.0 to i64
  %19 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %18
  store i32 %17, ptr %19, align 4
  %20 = add nsw i32 %.0, 1
  br label %21

21:                                               ; preds = %14
  %22 = icmp slt i32 %20, %0
  br i1 %22, label %14, label %.loopexit, !llvm.loop !32

.loopexit:                                        ; preds = %21
  br label %23

23:                                               ; preds = %.loopexit, %5
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_nested_siblings() #0 {
  %1 = alloca [100 x [100 x i32]], align 16
  br label %2

2:                                                ; preds = %28, %0
  %.0 = phi i32 [ 0, %0 ], [ %29, %28 ]
  %3 = icmp slt i32 %.0, 100
  br i1 %3, label %4, label %30

4:                                                ; preds = %2
  br label %5

5:                                                ; preds = %12, %4
  %.01 = phi i32 [ 0, %4 ], [ %13, %12 ]
  %6 = icmp slt i32 %.01, 100
  br i1 %6, label %7, label %14

7:                                                ; preds = %5
  %8 = sext i32 %.0 to i64
  %9 = getelementptr inbounds [100 x [100 x i32]], ptr %1, i64 0, i64 %8
  %10 = sext i32 %.01 to i64
  %11 = getelementptr inbounds [100 x i32], ptr %9, i64 0, i64 %10
  store i32 0, ptr %11, align 4
  br label %12

12:                                               ; preds = %7
  %13 = add nsw i32 %.01, 1
  br label %5, !llvm.loop !33

14:                                               ; preds = %5
  br label %15

15:                                               ; preds = %25, %14
  %.02 = phi i32 [ 0, %14 ], [ %26, %25 ]
  %16 = icmp slt i32 %.02, 100
  br i1 %16, label %17, label %27

17:                                               ; preds = %15
  %18 = add nsw i32 %.0, %.02
  %19 = sext i32 %.0 to i64
  %20 = getelementptr inbounds [100 x [100 x i32]], ptr %1, i64 0, i64 %19
  %21 = sext i32 %.02 to i64
  %22 = getelementptr inbounds [100 x i32], ptr %20, i64 0, i64 %21
  %23 = load i32, ptr %22, align 4
  %24 = add nsw i32 %23, %18
  store i32 %24, ptr %22, align 4
  br label %25

25:                                               ; preds = %17
  %26 = add nsw i32 %.02, 1
  br label %15, !llvm.loop !34

27:                                               ; preds = %15
  br label %28

28:                                               ; preds = %27
  %29 = add nsw i32 %.0, 1
  br label %2, !llvm.loop !35

30:                                               ; preds = %2
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_internal_control_flow(i32 noundef %0) #0 {
  %2 = alloca [100 x i32], align 16
  %3 = alloca [100 x i32], align 16
  br label %4

4:                                                ; preds = %16, %1
  %.01 = phi i32 [ 0, %1 ], [ %17, %16 ]
  %5 = icmp slt i32 %.01, %0
  br i1 %5, label %6, label %18

6:                                                ; preds = %4
  %7 = srem i32 %.01, 2
  %8 = icmp eq i32 %7, 0
  br i1 %8, label %9, label %12

9:                                                ; preds = %6
  %10 = sext i32 %.01 to i64
  %11 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %10
  store i32 10, ptr %11, align 4
  br label %15

12:                                               ; preds = %6
  %13 = sext i32 %.01 to i64
  %14 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %13
  store i32 20, ptr %14, align 4
  br label %15

15:                                               ; preds = %12, %9
  br label %16

16:                                               ; preds = %15
  %17 = add nsw i32 %.01, 1
  br label %4, !llvm.loop !36

18:                                               ; preds = %4
  br label %19

19:                                               ; preds = %33, %18
  %.0 = phi i32 [ 0, %18 ], [ %34, %33 ]
  %20 = icmp slt i32 %.0, %0
  br i1 %20, label %21, label %35

21:                                               ; preds = %19
  %22 = sext i32 %.0 to i64
  %23 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %22
  %24 = load i32, ptr %23, align 4
  %25 = icmp eq i32 %24, 10
  br i1 %25, label %26, label %29

26:                                               ; preds = %21
  %27 = sext i32 %.0 to i64
  %28 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %27
  store i32 1, ptr %28, align 4
  br label %32

29:                                               ; preds = %21
  %30 = sext i32 %.0 to i64
  %31 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %30
  store i32 0, ptr %31, align 4
  br label %32

32:                                               ; preds = %29, %26
  br label %33

33:                                               ; preds = %32
  %34 = add nsw i32 %.0, 1
  br label %19, !llvm.loop !37

35:                                               ; preds = %19
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_hoisted_preheader(i32 noundef %0) #0 {
  %2 = alloca [100 x i32], align 16
  %3 = alloca [100 x i32], align 16
  br label %4

4:                                                ; preds = %9, %1
  %.01 = phi i32 [ 0, %1 ], [ %10, %9 ]
  %5 = icmp slt i32 %.01, %0
  br i1 %5, label %6, label %11

6:                                                ; preds = %4
  %7 = sext i32 %.01 to i64
  %8 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %7
  store i32 %.01, ptr %8, align 4
  br label %9

9:                                                ; preds = %6
  %10 = add nsw i32 %.01, 1
  br label %4, !llvm.loop !38

11:                                               ; preds = %4
  %12 = mul nsw i32 %0, 5
  br label %13

13:                                               ; preds = %22, %11
  %.0 = phi i32 [ 0, %11 ], [ %23, %22 ]
  %14 = icmp slt i32 %.0, %0
  br i1 %14, label %15, label %24

15:                                               ; preds = %13
  %16 = sext i32 %.0 to i64
  %17 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %16
  %18 = load i32, ptr %17, align 4
  %19 = mul nsw i32 %18, %12
  %20 = sext i32 %.0 to i64
  %21 = getelementptr inbounds [100 x i32], ptr %3, i64 0, i64 %20
  store i32 %19, ptr %21, align 4
  br label %22

22:                                               ; preds = %15
  %23 = add nsw i32 %.0, 1
  br label %13, !llvm.loop !39

24:                                               ; preds = %13
  ret void
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 19.1.7 (++20250804090312+cd708029e0b2-1~exp1~20250804210325.79)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
!9 = distinct !{!9, !7}
!10 = distinct !{!10, !7}
!11 = distinct !{!11, !7}
!12 = distinct !{!12, !7}
!13 = distinct !{!13, !7}
!14 = distinct !{!14, !7}
!15 = distinct !{!15, !7}
!16 = distinct !{!16, !7}
!17 = distinct !{!17, !7}
!18 = distinct !{!18, !7}
!19 = distinct !{!19, !7}
!20 = distinct !{!20, !7}
!21 = distinct !{!21, !7}
!22 = distinct !{!22, !7}
!23 = distinct !{!23, !7}
!24 = distinct !{!24, !7}
!25 = distinct !{!25, !7}
!26 = distinct !{!26, !7}
!27 = distinct !{!27, !7}
!28 = distinct !{!28, !7}
!29 = distinct !{!29, !7}
!30 = distinct !{!30, !7}
!31 = distinct !{!31, !7}
!32 = distinct !{!32, !7}
!33 = distinct !{!33, !7}
!34 = distinct !{!34, !7}
!35 = distinct !{!35, !7}
!36 = distinct !{!36, !7}
!37 = distinct !{!37, !7}
!38 = distinct !{!38, !7}
!39 = distinct !{!39, !7}
