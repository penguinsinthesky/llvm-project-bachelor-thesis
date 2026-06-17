; RUN: opt -S -passes=xray-instrument-loops %s | FileCheck %s

; CHECK-DAG: @[[OUTER_REGION_NAME_GLOBAL:.+]] = private constant [{{[0-9]+}} x i8] c"my while loop\00"
; CHECK-DAG: @[[OUTER_REGION_GLOBAL:.+]] = internal constant { i64, ptr } { i64 2, ptr @[[OUTER_REGION_NAME_GLOBAL]] }, section "xray_custom_regions"

; CHECK-DAG: @[[INNER_REGION_NAME_GLOBAL:.+]] = private constant [{{[0-9]+}} x i8] c"my inner for loop\00"
; CHECK-DAG: @[[INNER_REGION_GLOBAL:.+]] = internal constant { i64, ptr } { i64 2, ptr @[[INNER_REGION_NAME_GLOBAL]] }, section "xray_custom_regions"

define i32 @main() {
entry:
  %a = alloca i32, align 4
  %i = alloca i32, align 4
  store i32 0, ptr %a, align 4
  br label %while.cond

while.cond:
  %0 = load i32, ptr %a, align 4
  %cmp = icmp slt i32 %0, 5
  br i1 %cmp, label %while.body, label %while.end

; CHECK-LABEL: while.body:
; CHECK-NEXT: call void @llvm.xray.customregionenter(ptr @[[OUTER_REGION_GLOBAL]])
; CHECK-NEXT: store i32 0, ptr %i, align 4
; CHECK-NEXT: br label %for.cond
while.body:
  store i32 0, ptr %i, align 4
  br label %for.cond

for.cond:
  %1 = load i32, ptr %i, align 4
  %cmp1 = icmp slt i32 %1, 5
  br i1 %cmp1, label %for.body, label %for.end

; CHECK-LABEL: for.body:
; CHECK-NEXT: call void @llvm.xray.customregionenter(ptr @[[INNER_REGION_GLOBAL]])
; CHECK-NEXT: call void @foo()
; CHECK-NEXT: br label %for.inc
for.body:
  call void @foo()
  br label %for.inc

; CHECK-LABEL: for.inc:
; CHECK-NEXT: %2 = load i32, ptr %i, align 4
; CHECK-NEXT: %inc = add nsw i32 %2, 1
; CHECK-NEXT: store i32 %inc, ptr %i, align 4
; CHECK-NEXT: call void @llvm.xray.customregionexit(ptr @[[INNER_REGION_GLOBAL]])
; CHECK-NEXT: br label %for.cond, !llvm.loop !0
for.inc:
  %2 = load i32, ptr %i, align 4
  %inc = add nsw i32 %2, 1
  store i32 %inc, ptr %i, align 4
  br label %for.cond, !llvm.loop !0

; CHECK-LABEL: for.end:
; CHECK-NEXT: %3 = load i32, ptr %a, align 4
; CHECK-NEXT: %inc2 = add nsw i32 %3, 1
; CHECK-NEXT: store i32 %inc2, ptr %a, align 4
; CHECK-NEXT: call void @llvm.xray.customregionexit(ptr @[[OUTER_REGION_GLOBAL]])
; CHECK-NEXT: br label %while.cond, !llvm.loop !2
for.end:
  %3 = load i32, ptr %a, align 4
  %inc2 = add nsw i32 %3, 1
  store i32 %inc2, ptr %a, align 4
  br label %while.cond, !llvm.loop !2

while.end:
  ret i32 0
}

declare void @foo()

; CHECK: declare void @llvm.xray.customregionenter(ptr)
; CHECK: declare void @llvm.xray.customregionexit(ptr)

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.xray.instrument", !"my inner for loop"}
!2 = distinct !{!2, !3}
!3 = !{!"llvm.loop.xray.instrument", !"my while loop"}
