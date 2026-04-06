; RUN: opt < %s -passes=xray-postinline-purge -S | FileCheck %s

; CHECK-LABEL: define void @foo(
define void @foo() {
  call void @llvm.xray.customregionenter(metadata !0)
; CHECK-NOT: call void @llvm.xray.customregionenter(metadata !{{[0-9]+}})
; CHECK-NEXT: %1 = add i32 42, 23
  %1 = add i32 42, 23
  call void @llvm.xray.customregionexit(metadata !0)
; CHECK-NOT: call void @llvm.xray.customregionexit(metadata !{{[0-9]+}})
; CHECK-NEXT: ret void
  ret void
}

!0 = !{i32 1, !1}
!1 = !{!"foo", ptr @foo}

