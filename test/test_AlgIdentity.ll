define dso_local i32 @foo() #0 {
  %1 = add nsw i32 0, 19
  %2 = add nsw i32 %1, 0
  %3 = mul nsw i32 %2, 1
  ret i32 %3
}