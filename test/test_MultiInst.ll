define dso_local i32 @main() {
  %1 = add nsw i32 0, 19
  %2 = add nsw i32 %1, 1
  %3 = sub nsw i32 %2, 1
  
  ret i32 %3
}