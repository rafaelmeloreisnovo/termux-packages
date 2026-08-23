.arch armv8-a
.global _start
.section .text
_start:
    mov x0, #0x0711
    ldr x1, =lane_stack_0_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_0
    mov x0, #0x0711
    ldr x1, =lane_stack_1_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_1
    mov x0, #0x0711
    ldr x1, =lane_stack_2_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_2
    mov x0, #0x0711
    ldr x1, =lane_stack_3_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_3
    mov x0, #0x0711
    ldr x1, =lane_stack_4_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_4
    mov x0, #0x0711
    ldr x1, =lane_stack_5_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_5
    mov x0, #0x0711
    ldr x1, =lane_stack_6_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_6
    mov x0, #0x0711
    ldr x1, =lane_stack_7_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_7
    mov x0, #0x0711
    ldr x1, =lane_stack_8_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_8
    mov x0, #0x0711
    ldr x1, =lane_stack_9_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_9
    mov x0, #0x0711
    ldr x1, =lane_stack_10_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_10
    mov x0, #0x0711
    ldr x1, =lane_stack_11_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_11
    mov x0, #0x0711
    ldr x1, =lane_stack_12_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_12
    mov x0, #0x0711
    ldr x1, =lane_stack_13_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_13
    mov x0, #0x0711
    ldr x1, =lane_stack_14_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_14
    mov x0, #0x0711
    ldr x1, =lane_stack_15_end
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #220
    .inst 0xd4000001
    cmp x0, #0
    b.eq .Lchild_15
    sub sp, sp, #16
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    mov x0, #1
    str w0, [sp]
    mov x0, #-1
    mov x1, sp
    mov x2, #0
    mov x3, #0
    mov x8, #260
    .inst 0xd4000001
    ldr x5, =lane_status
    mov w6, #0
    ldrb w0, [x5, #0]
    add w6, w6, w0
    ldrb w0, [x5, #1]
    add w6, w6, w0
    ldrb w0, [x5, #2]
    add w6, w6, w0
    ldrb w0, [x5, #3]
    add w6, w6, w0
    ldrb w0, [x5, #4]
    add w6, w6, w0
    ldrb w0, [x5, #5]
    add w6, w6, w0
    ldrb w0, [x5, #6]
    add w6, w6, w0
    ldrb w0, [x5, #7]
    add w6, w6, w0
    ldrb w0, [x5, #8]
    add w6, w6, w0
    ldrb w0, [x5, #9]
    add w6, w6, w0
    ldrb w0, [x5, #10]
    add w6, w6, w0
    ldrb w0, [x5, #11]
    add w6, w6, w0
    ldrb w0, [x5, #12]
    add w6, w6, w0
    ldrb w0, [x5, #13]
    add w6, w6, w0
    ldrb w0, [x5, #14]
    add w6, w6, w0
    ldrb w0, [x5, #15]
    add w6, w6, w0
    mov x0, #1
    mov x1, x5
    mov x2, #16
    mov x8, #64
    .inst 0xd4000001
    uxtw x0, w6
    mov x8, #94
    .inst 0xd4000001
.Lchild_0:
    mov x0, #-100
    ldr x1, =path_0
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #0]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_1:
    mov x0, #-100
    ldr x1, =path_1
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #1]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_2:
    mov x0, #-100
    ldr x1, =path_2
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #2]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_3:
    mov x0, #-100
    ldr x1, =path_3
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #3]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_4:
    mov x0, #-100
    ldr x1, =path_4
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #4]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_5:
    mov x0, #-100
    ldr x1, =path_5
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #5]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_6:
    mov x0, #-100
    ldr x1, =path_6
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #6]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_7:
    mov x0, #-100
    ldr x1, =path_7
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #7]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_8:
    mov x0, #-100
    ldr x1, =path_8
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #8]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_9:
    mov x0, #-100
    ldr x1, =path_9
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #9]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_10:
    mov x0, #-100
    ldr x1, =path_10
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #10]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_11:
    mov x0, #-100
    ldr x1, =path_11
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #11]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_12:
    mov x0, #-100
    ldr x1, =path_12
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #12]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_13:
    mov x0, #-100
    ldr x1, =path_13
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #13]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_14:
    mov x0, #-100
    ldr x1, =path_14
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #14]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.Lchild_15:
    mov x0, #-100
    ldr x1, =path_15
    mov x2, #0
    mov x3, #0
    mov x8, #56
    .inst 0xd4000001
    cmp x0, #0
    cset w4, lt
    ldr x5, =lane_status
    strb w4, [x5, #15]
    uxtw x0, w4
    mov x8, #93
    .inst 0xd4000001
.section .rodata
path_0: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/sh"
path_1: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/pkg"
path_2: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/apt"
path_3: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/apt-get"
path_4: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/dpkg"
path_5: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/bash"
path_6: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/busybox"
path_7: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/proot"
path_8: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/termux-battery-status"
path_9: .asciz "/data/data/com.termux.rafacodephi/files/usr/bin/termux-sensor"
path_10: .asciz "/data/data/com.termux.rafacodephi/files/usr/libexec/termux-api"
path_11: .asciz "/data/data/com.termux.rafacodephi/files/usr/libexec/termux-api-broadcast"
path_12: .asciz "/data/data/com.termux.rafacodephi/files/usr/etc/apt/sources.list.d/termux.sources"
path_13: .asciz "/data/data/com.termux.rafacodephi/files/usr/etc/apt/apt.conf.d/00rafcodephi-repository-block"
path_14: .asciz "/data/data/com.termux.rafacodephi/files/usr/var/lib/dpkg/status"
path_15: .asciz "/data/data/com.termux.rafacodephi/files/usr/BOOTSTRAP_INFO"
.section .bss
.balign 16
lane_status: .skip 16
.balign 16
lane_stack_0: .skip 256
lane_stack_0_end:
.balign 16
lane_stack_1: .skip 256
lane_stack_1_end:
.balign 16
lane_stack_2: .skip 256
lane_stack_2_end:
.balign 16
lane_stack_3: .skip 256
lane_stack_3_end:
.balign 16
lane_stack_4: .skip 256
lane_stack_4_end:
.balign 16
lane_stack_5: .skip 256
lane_stack_5_end:
.balign 16
lane_stack_6: .skip 256
lane_stack_6_end:
.balign 16
lane_stack_7: .skip 256
lane_stack_7_end:
.balign 16
lane_stack_8: .skip 256
lane_stack_8_end:
.balign 16
lane_stack_9: .skip 256
lane_stack_9_end:
.balign 16
lane_stack_10: .skip 256
lane_stack_10_end:
.balign 16
lane_stack_11: .skip 256
lane_stack_11_end:
.balign 16
lane_stack_12: .skip 256
lane_stack_12_end:
.balign 16
lane_stack_13: .skip 256
lane_stack_13_end:
.balign 16
lane_stack_14: .skip 256
lane_stack_14_end:
.balign 16
lane_stack_15: .skip 256
lane_stack_15_end:
