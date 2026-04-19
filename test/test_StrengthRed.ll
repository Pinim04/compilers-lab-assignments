define dso_local noundef i32 @main() #0 {
  %1 = srem i32 10, 2
  %2 = mul nsw i32 %1, 17
  %3 = mul nsw i32 %2, 15
  %4 = sdiv i32 %3, 8
  %5 = sdiv i32 %4, 7
  ret i32 0
}