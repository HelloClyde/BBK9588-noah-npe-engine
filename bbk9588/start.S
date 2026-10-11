.set noreorder
.set noat
.section .text.bda_start,"ax",@progbits
.globl bda_start
.ent bda_start
bda_start:
    lui $t0,%hi(__bss_start)
    addiu $t0,$t0,%lo(__bss_start)
    lui $t1,%hi(__bss_end)
    addiu $t1,$t1,%lo(__bss_end)
1:
    beq $t0,$t1,2f
    nop
    sb $zero,0($t0)
    b 1b
    addiu $t0,$t0,1
2:
    j bda_main
    nop
.end bda_start
