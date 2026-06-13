; RUN: opt -S -passes=xray-instrument-loops %s | FileCheck %s

; CHECK: @[[REGION_NAME_GLOBAL:[a-zA-Z0-9_\" ]+]] = private constant [{{[0-9]+}} x i8]
; CHECK: @[[REGION_GLOBAL:[a-zA-Z0-9_\" ]+]] = internal constant { i64, ptr } { i64 2, ptr @[[REGION_NAME_GLOBAL]] }, section "xray_custom_regions"

define void @foo() {
entry:
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %for.cond

for.cond:
  %0 = load i32, ptr %i, align 4
  %cmp = icmp slt i32 %0, 5
  br i1 %cmp, label %for.body, label %for.end

; CHECK-LABEL: for.body:
for.body:
  ; CHECK-NEXT: call void @llvm.xray.customregionenter(ptr @[[REGION_GLOBAL]])
  %1 = load i32, ptr %i, align 4
  ; CHECK-NEXT: %1 = load i32, ptr %i, align 4
  %2 = load i32, ptr %i, align 4
  ; CHECK-NEXT: %2 = load i32, ptr %i, align 4
  %add = add nsw i32 %1, %2
  ; CHECK-NEXT: %add = add nsw i32 %1, %2
  br label %for.inc
  ; CHECK-NEXT: br label %for.inc

; CHECK-LABEL: for.inc:
for.inc:
  %3 = load i32, ptr %i, align 4
  ; CHECK-NEXT: %3 = load i32, ptr %i, align 4
  %inc = add nsw i32 %3, 1
  ; CHECK-NEXT: %inc = add nsw i32 %3, 1
  store i32 %inc, ptr %i, align 4
  ; CHECK-NEXT: store i32 %inc, ptr %i, align 4
  ; CHECK-NEXT: call void @llvm.xray.customregionexit(ptr @[[REGION_GLOBAL]])
  br label %for.cond, !llvm.loop !0
  ; CHECK-NEXT: br label %for.cond, !llvm.loop !{{[0-9]+}}

for.end:
  ret void
}

; CHECK: declare void @llvm.xray.customregionenter(ptr)
; CHECK: declare void @llvm.xray.customregionexit(ptr)

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.xray.instrument", !"my for loop"}



