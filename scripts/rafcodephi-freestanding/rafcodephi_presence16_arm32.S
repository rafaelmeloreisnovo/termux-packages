.syntax unified
.arm
.arch armv7-a
.global _start
.section .text
_start:
    movw r0, #0x0711
    ldr r1, =lane_stack_0_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_0
    movw r0, #0x0711
    ldr r1, =lane_stack_1_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_1
    movw r0, #0x0711
    ldr r1, =lane_stack_2_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_2
    movw r0, #0x0711
    ldr r1, =lane_stack_3_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_3
    movw r0, #0x0711
    ldr r1, =lane_stack_4_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_4
    movw r0, #0x0711
    ldr r1, =lane_stack_5_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_5
    movw r0, #0x0711
    ldr r1, =lane_stack_6_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_6
    movw r0, #0x0711
    ldr r1, =lane_stack_7_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_7
    movw r0, #0x0711
    ldr r1, =lane_stack_8_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_8
    movw r0, #0x0711
    ldr r1, =lane_stack_9_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_9
    movw r0, #0x0711
    ldr r1, =lane_stack_10_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_10
    movw r0, #0x0711
    ldr r1, =lane_stack_11_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_11
    movw r0, #0x0711
    ldr r1, =lane_stack_12_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_12
    movw r0, #0x0711
    ldr r1, =lane_stack_13_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_13
    movw r0, #0x0711
    ldr r1, =lane_stack_14_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_14
    movw r0, #0x0711
    ldr r1, =lane_stack_15_end
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r7, #120
    .word 0xef000000
    cmp r0, #0
    beq .Lchild_15
    sub sp, sp, #16
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    mov r0, #1
    str r0, [sp]
    mvn r0, #0
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r7, #114
    .word 0xef000000
    ldr r5, =lane_status
    mov r6, #0
    ldrb r0, [r5, #0]
    add r6, r6, r0
    ldrb r0, [r5, #1]
    add r6, r6, r0
    ldrb r0, [r5, #2]
    add r6, r6, r0
    ldrb r0, [r5, #3]
    add r6, r6, r0
    ldrb r0, [r5, #4]
    add r6, r6, r0
    ldrb r0, [r5, #5]
    add r6, r6, r0
    ldrb r0, [r5, #6]
    add r6, r6, r0
    ldrb r0, [r5, #7]
    add r6, r6, r0
    ldrb r0, [r5, #8]
    add r6, r6, r0
    ldrb r0, [r5, #9]
    add r6, r6, r0
    ldrb r0, [r5, #10]
    add r6, r6, r0
    ldrb r0, [r5, #11]
    add r6, r6, r0
    ldrb r0, [r5, #12]
    add r6, r6, r0
    ldrb r0, [r5, #13]
    add r6, r6, r0
    ldrb r0, [r5, #14]
    add r6, r6, r0
    ldrb r0, [r5, #15]
    add r6, r6, r0
    mov r0, #1
    mov r1, r5
    mov r2, #16
    mov r7, #4
    .word 0xef000000
    mov r0, r6
    mov r7, #248
    .word 0xef000000
.Lchild_0:
    mvn r0, #99
    ldr r1, =path_0
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #0]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_1:
    mvn r0, #99
    ldr r1, =path_1
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #1]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_2:
    mvn r0, #99
    ldr r1, =path_2
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #2]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_3:
    mvn r0, #99
    ldr r1, =path_3
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #3]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_4:
    mvn r0, #99
    ldr r1, =path_4
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #4]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_5:
    mvn r0, #99
    ldr r1, =path_5
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #5]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_6:
    mvn r0, #99
    ldr r1, =path_6
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #6]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_7:
    mvn r0, #99
    ldr r1, =path_7
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #7]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_8:
    mvn r0, #99
    ldr r1, =path_8
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #8]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_9:
    mvn r0, #99
    ldr r1, =path_9
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #9]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_10:
    mvn r0, #99
    ldr r1, =path_10
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #10]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_11:
    mvn r0, #99
    ldr r1, =path_11
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #11]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_12:
    mvn r0, #99
    ldr r1, =path_12
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #12]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_13:
    mvn r0, #99
    ldr r1, =path_13
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #13]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_14:
    mvn r0, #99
    ldr r1, =path_14
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #14]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
.Lchild_15:
    mvn r0, #99
    ldr r1, =path_15
    mov r2, #0
    mov r3, #0
    movw r7, #322
    .word 0xef000000
    cmp r0, #0
    mov r4, #0
    movlt r4, #1
    ldr r5, =lane_status
    strb r4, [r5, #15]
    mov r0, r4
    mov r7, #1
    .word 0xef000000
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
