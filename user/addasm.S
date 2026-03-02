.section .text
.globl main

main:
    addi sp, sp, -64
    mv s0, sp

    mv a0, s0
    li a1, 64
    call gets
    
    beqz a0, err_read

    mv t0, s0

rm_newline:
    lb t1, 0(t0)
    li t2, 10
    beq t1, t2, fd_newline
    beqz t1, after_rm
    addi t0, t0, 1
    j rm_newline

fd_newline:
    sb zero, 0(t0)

after_rm:
    la a0, str_with_bracs
    mv a1, s0
    call printf

    lb t0, 0(s0)
    beqz t0, err_empty

    mv t0, s0

find_sp:
    lb t1, 0(t0)
    beqz t1, err_no_space
    li t2, 32
    beq t1, t2, fd_sp
    addi t0, t0, 1
    j find_sp

fd_sp:
    beq t0, s0, err_first_space

    addi t2, t0, 1
    lb t3, 0(t2)
    beqz t3, err_no_second
    li t4, 32
    beq t3, t4, err_double_space

    sb zero, 0(t0)

    mv a0, s0
    call atoi
    mv s1, a0

    mv a0, t2
    call atoi
    mv s2, a0

    mv a0, s1
    mv a1, s2
    call add

    mv a3, a0
    mv a1, s1
    mv a2, s2
    la a0, sum_res
    call printf

    addi sp, sp, 64
    li a0, 0
    call exit

err_read:
    li a0, 2
    la a1, read_err
    call fprintf
    addi sp, sp, 64
    li a0, 1
    call exit

err_empty:
    li a0, 2
    la a1, empty_err
    call fprintf
    addi sp, sp, 64
    li a0, 1
    call exit

err_first_space:
    li a0, 2
    la a1, first_space_err
    call fprintf
    addi sp, sp, 64
    li a0, 1
    call exit

err_no_space:
    la a0, no_space_err
    call printf
    addi sp, sp, 64
    li a0, 1
    call exit

err_no_second:
    li a0, 2
    la a1, no_second_err
    call fprintf
    addi sp, sp, 64
    li a0, 1
    call exit

err_double_space:
    li a0, 2
    la a1, double_space_err
    call fprintf
    addi sp, sp, 64
    li a0, 1
    call exit

.section .rodata
str_with_bracs:
    .string "|%s|\n"
sum_res:
    .string "%d + %d = %d\n"
read_err:
    .string "Error in reading\n"
empty_err:
    .string "Error: empty string\n"
first_space_err:
    .string "Error: first symbol is space.\n"
no_space_err:
    .string "Error: no space after first number.\n" 
no_second_err:
    .string "Error: no second number.\n"
double_space_err:
    .string "Error: double space.\n"