; RUN: opt -S -passes=xray-instrument-loops %s | FileCheck %s

; CHECK: @[[REGION_NAME_GLOBAL:.+]] = private constant [{{[0-9]+}} x i8]
; CHECK: @[[REGION_GLOBAL:.+]] = internal constant { i64, ptr } { i64 2, ptr @[[REGION_NAME_GLOBAL]] }, section "xray_custom_regions"

define i32 @main() {
entry:
  %a = alloca i32, align 4
  store i32 0, ptr %a, align 4
  br label %while.cond
while.cond:
  %0 = load i32, ptr %a, align 4
  %cmp = icmp slt i32 %0, 5
  br i1 %cmp, label %while.body, label %while.end

; CHECK-LABEL: while.body:
while.body:
; CHECK-NEXT: call void @llvm.xray.customregionenter(ptr @[[REGION_GLOBAL]])
  call void @foo()
; CHECK-NEXT: call void @foo()
  %1 = load i32, ptr %a, align 4
; CHECK-NEXT: %1 = load i32, ptr %a, align 4
  %inc = add nsw i32 %1, 1
; CHECK-NEXT: %inc = add nsw i32 %1, 1
  store i32 %inc, ptr %a, align 4
; CHECK-NEXT: store i32 %inc, ptr %a, align 4
; CHECK-NEXT: call void @llvm.xray.customregionexit(ptr @[[REGION_GLOBAL]])
  br label %while.cond, !llvm.loop !0
; CHECK-NEXT: br label %while.cond, !llvm.loop !0
while.end:
  ret i32 0
}

declare void @foo()

; CHECK: declare void @llvm.xray.customregionenter(ptr)
; CHECK: declare void @llvm.xray.customregionexit(ptr)

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.xray.instrument", !"my while loop"}
