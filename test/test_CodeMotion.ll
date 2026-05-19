; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z10test_basiciii(i32 noundef %0, i32 noundef %1, i32 noundef %2) #0 {
  br label %4

4:                                                ; preds = %10, %3
  %.01 = phi i32 [ 0, %3 ], [ %9, %10 ]
  %.0 = phi i32 [ 0, %3 ], [ %11, %10 ]
  %5 = icmp slt i32 %.0, %2
  br i1 %5, label %6, label %12

6:                                                ; preds = %4
  %7 = mul nsw i32 %0, %1
  %8 = add nsw i32 %7, %.0
  %9 = add nsw i32 %.01, %8
  br label %10

10:                                               ; preds = %6
  %11 = add nsw i32 %.0, 1
  br label %4

12:                                               ; preds = %4
  ret i32 %.01
}

; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z10test_chainiiii(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) #0 {
  br label %5

5:                                                ; preds = %13, %4
  %.01 = phi i32 [ 0, %4 ], [ %12, %13 ]
  %.0 = phi i32 [ 0, %4 ], [ %14, %13 ]
  %6 = icmp slt i32 %.0, %3
  br i1 %6, label %7, label %15

7:                                                ; preds = %5
  %8 = add nsw i32 %0, %1
  %9 = mul nsw i32 %8, %2
  %10 = shl i32 %9, 2
  %11 = add nsw i32 %10, %.0
  %12 = add nsw i32 %.01, %11
  br label %13

13:                                               ; preds = %7
  %14 = add nsw i32 %.0, 1
  br label %5

15:                                               ; preds = %5
  ret i32 %.01
}

; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z11test_memoryPii(ptr noundef %0, i32 noundef %1) #0 {
  br label %3

3:                                                ; preds = %9, %2
  %.01 = phi i32 [ 0, %2 ], [ %8, %9 ]
  %.0 = phi i32 [ 0, %2 ], [ %10, %9 ]
  %4 = icmp slt i32 %.0, %1
  br i1 %4, label %5, label %11

5:                                                ; preds = %3
  %6 = load i32, ptr %0, align 4
  %7 = add nsw i32 %6, %.0
  %8 = add nsw i32 %.01, %7
  br label %9

9:                                                ; preds = %5
  %10 = add nsw i32 %.0, 1
  br label %3

11:                                               ; preds = %3
  ret i32 %.01
}

; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z14test_dominanceiiii(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) #0 {
  br label %5

5:                                                ; preds = %13, %4
  %.02 = phi i32 [ 0, %4 ], [ %8, %13 ]
  %.01 = phi i32 [ 0, %4 ], [ %.1, %13 ]
  %.0 = phi i32 [ 0, %4 ], [ %14, %13 ]
  %6 = icmp slt i32 %.0, %2
  br i1 %6, label %7, label %15

7:                                                ; preds = %5
  %8 = add nsw i32 %.02, %.0
  %9 = icmp sgt i32 %3, 5
  br i1 %9, label %10, label %12

10:                                               ; preds = %7
  %11 = sdiv i32 %0, %1
  br label %12

12:                                               ; preds = %10, %7
  %.1 = phi i32 [ %11, %10 ], [ %.01, %7 ]
  br label %13

13:                                               ; preds = %12
  %14 = add nsw i32 %.0, 1
  br label %5

15:                                               ; preds = %5
  %16 = add nsw i32 %.02, %.01
  ret i32 %16
}

; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z17test_nested_basiciiii(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) #0 {
  br label %5

5:                                                ; preds = %18, %4
  %.02 = phi i32 [ 0, %4 ], [ %.1, %18 ]
  %.01 = phi i32 [ 0, %4 ], [ %19, %18 ]
  %6 = icmp slt i32 %.01, %2
  br i1 %6, label %7, label %20

7:                                                ; preds = %5
  %8 = mul nsw i32 %0, %1
  br label %9

9:                                                ; preds = %15, %7
  %.1 = phi i32 [ %.02, %7 ], [ %14, %15 ]
  %.0 = phi i32 [ 0, %7 ], [ %16, %15 ]
  %10 = icmp slt i32 %.0, %3
  br i1 %10, label %11, label %17

11:                                               ; preds = %9
  %12 = add nsw i32 %8, %.01
  %13 = add nsw i32 %12, %.0
  %14 = add nsw i32 %.1, %13
  br label %15

15:                                               ; preds = %11
  %16 = add nsw i32 %.0, 1
  br label %9

17:                                               ; preds = %9
  br label %18

18:                                               ; preds = %17
  %19 = add nsw i32 %.01, 1
  br label %5

20:                                               ; preds = %5
  ret i32 %.02
}

; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local noundef i32 @_Z16test_nested_trapiii(i32 noundef %0, i32 noundef %1, i32 noundef %2) #0 {
  br label %4

4:                                                ; preds = %15, %3
  %.03 = phi i32 [ 0, %3 ], [ %14, %15 ]
  %.02 = phi i32 [ 0, %3 ], [ %16, %15 ]
  %5 = icmp slt i32 %.02, %1
  br i1 %5, label %6, label %17

6:                                                ; preds = %4
  br label %7

7:                                                ; preds = %11, %6
  %.01 = phi i32 [ %0, %6 ], [ %10, %11 ]
  %.0 = phi i32 [ 0, %6 ], [ %12, %11 ]
  %8 = icmp slt i32 %.0, %2
  br i1 %8, label %9, label %13

9:                                                ; preds = %7
  %10 = add nsw i32 %.01, 5
  br label %11

11:                                               ; preds = %9
  %12 = add nsw i32 %.0, 1
  br label %7

13:                                               ; preds = %7
  %14 = add nsw i32 %.03, %.01
  br label %15

15:                                               ; preds = %13
  %16 = add nsw i32 %.02, 1
  br label %4

17:                                               ; preds = %4
  ret i32 %.03
}