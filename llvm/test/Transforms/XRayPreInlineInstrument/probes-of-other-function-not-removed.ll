; RUN: opt < %s -passes=xray-postinline-purge -S | FileCheck %s

; CHECK-LABEL: define void @foo(
define void @foo() {
  call void @llvm.xray.customregionenter(metadata !0)
; CHECK-NEXT: call void @llvm.xray.customregionenter(metadata !0)
; CHECK-NEXT: %1 = add i32 42, 23
  %1 = add i32 42, 23
  call void @llvm.xray.customregionexit(metadata !0)
; CHECK-NEXT: call void @llvm.xray.customregionexit(metadata !0)
; CHECK-NEXT: ret void
  ret void
}

define void @other_func() {
    ret void
}

!0 = !{i32 1, !1}
!1 = !{!"other_func", ptr @other_func}

