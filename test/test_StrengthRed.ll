define dso_local noundef i32 @_Z23strength_reduction_testi(i32 noundef %0) #0 {
  %2 = mul nsw i32 %0, 15
  %3 = mul nsw i32 %2, 3
  %4 = mul nsw i32 %3, 6
  %5 = mul nsw i32 %4, -15
  %6 = sdiv i32 %5, %5
  %7 = sdiv i32 %6, 16
  %8 = mul nsw i32 %5, %5
  ret i32 %7
}

define dso_local noundef i32 @main() #1 {
  %1 = call noundef i32 @_Z23strength_reduction_testi(i32 noundef 3)
  ret i32 %1
}
