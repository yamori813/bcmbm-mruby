	.globl  main
	.globl  _init

	.text
	.globl	start
	.ent	start
	.set	reorder
start:
	li      $sp, 0x80800000
	nop
	/* allocate space for result */
	addiu	$sp, $sp, -8
	jal	main
	/* store the result */
	sw	$2, 4($sp)
	addiu	$sp, $sp, 8
loop:	
	j	loop
	.end start

