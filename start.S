# Copyright (c) 2018 Hiroki Mori. All rights reserved.

	.globl  main
	.globl  relocsp

	.text
	.globl	start
	.ent	start
	.set	reorder
start:
	li      $sp, 0x80800000
	nop
# bss clear
	la	$8,_fbss
	la	$9,_end
1:	sw	$0,0($8)
	add	$8,4
	blt	$8,$9,1b
	/* allocate space for result */
	addiu	$sp, $sp, -8
	jal	main
	/* store the result */
	sw	$2, 4($sp)
	addiu	$sp, $sp, 8
loop:	
	j	loop
	.end start

relocsp:
	move	$8, $sp
	li	$9, 0x80800000
	move	$10, $sp
	add	$10, $10, $4
2:	lw	$11, 0($8)
	sw	$11, 0($10)
	add	$8, 4
	add	$10, 4
	blt	$8,$9,2b
	add	$sp, $sp, $4
	move	$2, $sp
	j	$ra
