; RUN: opt -S -passes=xray-instrument-loops %s | FileCheck %s

; CHECK: @[[REGION_NAME_GLOBAL:.+]] = private constant [{{[0-9]+}} x i8]
; CHECK: @[[REGION_GLOBAL:.+]] = internal constant { i64, ptr } { i64 2, ptr @[[REGION_NAME_GLOBAL]] }, section "xray_custom_regions"

define i32 @main() {
entry:
  %b = alloca i32, align 4
  store i32 0, ptr %b, align 4
  br label %do.body
; CHECK-LABEL: do.body:
do.body:
; CHECK-NEXT: call void @llvm.xray.customregionenter(ptr @[[REGION_GLOBAL]])
  call void @foo()
; CHECK-NEXT: call void @foo()
  %0 = load i32, ptr %b, align 4
; CHECK-NEXT: %0 = load i32, ptr %b, align 4
  %inc = add nsw i32 %0, 1
; CHECK-NEXT: %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %b, align 4
; CHECK-NEXT: store i32 %inc, ptr %b, align 4
  br label %do.cond
; CHECK-NEXT: br label %do.cond

; CHECK-LABEL: do.cond:
do.cond:
  %1 = load i32, ptr %b, align 4
; CHECK-NEXT: %1 = load i32, ptr %b, align 4
  %cmp = icmp sle i32 %1, 5
; CHECK-NEXT: %cmp = icmp sle i32 %1, 5
; CHECK-NEXT: call void @llvm.xray.customregionexit(ptr @[[REGION_GLOBAL]])
  br i1 %cmp, label %do.body, label %do.end, !llvm.loop !0
; CHECK-NEXT: br i1 %cmp, label %do.body, label %do.end, !llvm.loop !0

do.end:                                           
  ret i32 0
}

declare void @foo()

; CHECK: declare void @llvm.xray.customregionenter(ptr)
; CHECK: declare void @llvm.xray.customregionexit(ptr)

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.xray.instrument", !"my do-while loop"}
