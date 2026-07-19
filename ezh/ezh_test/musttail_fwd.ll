; Vararg ellipsis forwarding cannot be spelled in C, so this one shape of
; the musttail self-test lives in IR: the unnamed arguments of vfwd are
; forwarded to vsum through the entry-captured argument registers.
declare i32 @vsum(i32, ...)
define i32 @vfwd(i32 %n, ...) {
  %r = musttail call i32 (i32, ...) @vsum(i32 %n, ...)
  ret i32 %r
}
