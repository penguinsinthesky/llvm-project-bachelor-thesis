; RUN: opt -S -passes=xray-instrument-loops %s | FileCheck %s

; CHECK-DAG: @[[OUTER_REGION_NAME_GLOBAL:.+]] = private constant [{{[0-9]+}} x i8] c"my for loop\00"
; CHECK-DAG: @[[OUTER_REGION_GLOBAL:.+]] = internal constant { i64, ptr } { i64 2, ptr @[[OUTER_REGION_NAME_GLOBAL]] }, section "xray_custom_regions"

; CHECK-DAG: @[[INNER_REGION_NAME_GLOBAL:.+]] = private constant [{{[0-9]+}} x i8] c"my inner do-while loop\00"
; CHECK-DAG: @[[INNER_REGION_GLOBAL:.+]] = internal constant { i64, ptr } { i64 2, ptr @[[INNER_REGION_NAME_GLOBAL]] }, section "xray_custom_regions"

define i32 @main() {
entry:
  %i = alloca i32, align 4
  %b = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %for.cond

; CHECK-LABEL: for.cond:
; CHECK-NEXT: %0 = load i32, ptr %i, align 4
; CHECK-NEXT: %cmp = icmp slt i32 %0, 5
; CHECK-NEXT: br i1 %cmp, label %for.body, label %for.end
for.cond:
  %0 = load i32, ptr %i, align 4
  %cmp = icmp slt i32 %0, 5
  br i1 %cmp, label %for.body, label %for.end

; CHECK-LABEL: for.body:
; CHECK-NEXT: call void @llvm.xray.customregionenter(ptr @[[OUTER_REGION_GLOBAL]])
; CHECK-NEXT: store i32 0, ptr %b, align 4
; CHECK-NEXT: br label %do.body
for.body:
  store i32 0, ptr %b, align 4
  br label %do.body

; CHECK-LABEL: do.body:
; CHECK-NEXT: call void @llvm.xray.customregionenter(ptr @[[INNER_REGION_GLOBAL]])
; CHECK-NEXT: call void @foo()
; CHECK-NEXT: %1 = load i32, ptr %b, align 4
; CHECK-NEXT: %inc = add nsw i32 %1, 1
; CHECK-NEXT: store i32 %inc, ptr %b, align 4
; CHECK-NEXT: br label %do.cond
do.body:
  call void @foo()
  %1 = load i32, ptr %b, align 4
  %inc = add nsw i32 %1, 1
  store i32 %inc, ptr %b, align 4
  br label %do.cond

; CHECK-LABEL: do.cond:
; CHECK-NEXT: %2 = load i32, ptr %b, align 4
; CHECK-NEXT: %cmp1 = icmp sle i32 %2, 5
; CHECK-NEXT: call void @llvm.xray.customregionexit(ptr @[[INNER_REGION_GLOBAL]])
; CHECK-NEXT: br i1 %cmp1, label %do.body, label %do.end, !llvm.loop !0
do.cond:
  %2 = load i32, ptr %b, align 4
  %cmp1 = icmp sle i32 %2, 5
  br i1 %cmp1, label %do.body, label %do.end, !llvm.loop !0

; CHECK-LABEL: do.end:
; CHECK-NEXT: br label %for.inc
do.end:
  br label %for.inc

; CHECK-LABEL: for.inc:
; CHECK-NEXT: %3 = load i32, ptr %i, align 4
; CHECK-NEXT: %inc2 = add nsw i32 %3, 1
; CHECK-NEXT: store i32 %inc2, ptr %i, align 4
; CHECK-NEXT: call void @llvm.xray.customregionexit(ptr @[[OUTER_REGION_GLOBAL]])
; CHECK-NEXT: br label %for.cond, !llvm.loop !2
for.inc:
  %3 = load i32, ptr %i, align 4
  %inc2 = add nsw i32 %3, 1
  store i32 %inc2, ptr %i, align 4
  br label %for.cond, !llvm.loop !2

; CHECK-LABEL: for.end:
; CHECK-NEXT: ret i32 0
for.end:
  ret i32 0
}

declare void @foo()

; CHECK: declare void @llvm.xray.customregionenter(ptr)
; CHECK: declare void @llvm.xray.customregionexit(ptr)

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.xray.instrument", !"my inner do-while loop"}
!2 = distinct !{!2, !3}
!3 = !{!"llvm.loop.xray.instrument", !"my for loop"}
