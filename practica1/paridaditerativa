.data
	msj1: .asciiz "Ingrese un Numero: "
	salto: .asciiz "\n"
	msj2: .asciiz "El resultado es: "
.text

	main:
	
	la $a0, msj1
	li $v0, 4
	syscall
	
	la $a0, salto
	li $v0, 4
	syscall
	
	li $v0, 5
	syscall
	move $t0, $v0
	
	move $t1, $zero
	li $t2, 1
	
	while:
	beq $t0, $zero, resultado
	add $t1, $t1, $t2
	subi $t0, $t0, 1
	neg $t2, $t2
	j while
	
	
	resultado:
	la $a0, msj2
	li $v0, 4
	syscall
	
	move $a0, $t1
    	li $v0, 1
    	syscall

    	la $a0, salto
    	li $v0, 4
    	syscall

    	li $v0, 10
    	syscall
	
	