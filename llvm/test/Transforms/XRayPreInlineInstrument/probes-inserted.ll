; RUN: opt < %s -passes=xray-preinline-instrument -S | FileCheck %s

; CHECK-LABEL: define void @foo(
define void @foo() {
; CHECK-NEXT: call void @llvm.xray.customregionenter(metadata ![[MD:[0-9]+]])
; CHECK-NEXT: %1 = add i32 42, 23
  %1 = add i32 42, 23
; CHECK-NEXT: call void @llvm.xray.customregionexit(metadata ![[MD]])
  ret void
}

; CHECK: ![[MD]] = !{i32 1, ![[MD_INLINED:[0-9]+]]}
; CHECK-NEXT: ![[MD_INLINED]] = !{!"foo", ptr @foo}
