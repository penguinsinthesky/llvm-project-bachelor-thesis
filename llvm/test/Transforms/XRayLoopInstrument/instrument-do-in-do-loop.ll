; RUN: opt -S -passes=xray-instrument-loops %s | FileCheck %s

; CHECK-DAG: @[[OUTER_REGION_NAME_GLOBAL:.+]] = private constant [{{[0-9]+}} x i8] c"my outer do-while loop\00"
; CHECK-DAG: @[[OUTER_REGION_GLOBAL:.+]] = internal constant { i64, ptr } { i64 2, ptr @[[OUTER_REGION_NAME_GLOBAL]] }, section "xray_custom_regions"

; CHECK-DAG: @[[INNER_REGION_NAME_GLOBAL:.+]] = private constant [{{[0-9]+}} x i8] c"my inner do-while loop\00"
; CHECK-DAG: @[[INNER_REGION_GLOBAL:.+]] = internal constant { i64, ptr } { i64 2, ptr @[[INNER_REGION_NAME_GLOBAL]] }, section "xray_custom_regions"

define i32 @main() {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  store i32 0, ptr %a, align 4
  br label %do.body

; CHECK-LABEL: do.body:
; CHECK-NEXT: call void @llvm.xray.customregionenter(ptr @[[OUTER_REGION_GLOBAL]])
; CHECK-NEXT: store i32 0, ptr %b, align 4
; CHECK-NEXT: br label %do.body1
do.body:
  store i32 0, ptr %b, align 4
  br label %do.body1

; CHECK-LABEL: do.body1:
; CHECK-NEXT: call void @llvm.xray.customregionenter(ptr @[[INNER_REGION_GLOBAL]])
; CHECK-NEXT: call void @foo()
; CHECK-NEXT: %0 = load i32, ptr %b, align 4
; CHECK-NEXT: %inc = add nsw i32 %0, 1
; CHECK-NEXT: store i32 %inc, ptr %b, align 4
; CHECK-NEXT: br label %do.cond
do.body1:
  call void @foo()
  %0 = load i32, ptr %b, align 4
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %b, align 4
  br label %do.cond

; CHECK-LABEL: do.cond:
; CHECK-NEXT: %1 = load i32, ptr %b, align 4
; CHECK-NEXT: %cmp = icmp sle i32 %1, 5
; CHECK-NEXT: call void @llvm.xray.customregionexit(ptr @[[INNER_REGION_GLOBAL]])
; CHECK-NEXT: br i1 %cmp, label %do.body1, label %do.end, !llvm.loop !0
do.cond:
  %1 = load i32, ptr %b, align 4
  %cmp = icmp sle i32 %1, 5
  br i1 %cmp, label %do.body1, label %do.end, !llvm.loop !0

do.end:
  %2 = load i32, ptr %a, align 4
  %inc2 = add nsw i32 %2, 1
  store i32 %inc2, ptr %a, align 4
  br label %do.cond3

; CHECK-LABEL: do.cond3:
; CHECK-NEXT: %3 = load i32, ptr %a, align 4
; CHECK-NEXT: %cmp4 = icmp sle i32 %3, 5
; CHECK-NEXT: call void @llvm.xray.customregionexit(ptr @[[OUTER_REGION_GLOBAL]])
; CHECK-NEXT: br i1 %cmp4, label %do.body, label %do.end5, !llvm.loop !2
do.cond3:
  %3 = load i32, ptr %a, align 4
  %cmp4 = icmp sle i32 %3, 5
  br i1 %cmp4, label %do.body, label %do.end5, !llvm.loop !2

do.end5:
  ret i32 0
}

declare void @foo()

; CHECK: declare void @llvm.xray.customregionenter(ptr)
; CHECK: declare void @llvm.xray.customregionexit(ptr)

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.xray.instrument", !"my inner do-while loop"}
!2 = distinct !{!2, !3}
!3 = !{!"llvm.loop.xray.instrument", !"my outer do-while loop"}
