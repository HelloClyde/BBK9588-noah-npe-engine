.set noreorder
.set noat
.text
.globl npe_bbk_on_stack
.ent npe_bbk_on_stack
npe_bbk_on_stack:
    addiu $sp,$sp,-24
    sw $ra,20($sp)
    sw $s0,16($sp)
    move $s0,$sp
    move $sp,$a0
    addiu $sp,$sp,-32
    jalr $a1
    nop
    move $sp,$s0
    lw $s0,16($sp)
    lw $ra,20($sp)
    jr $ra
    addiu $sp,$sp,24
.end npe_bbk_on_stack
