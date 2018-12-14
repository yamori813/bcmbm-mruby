# Copyright (c) 2018 Hiroki Mori. All rights reserved.

	.globl  main
	.globl  _init

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

