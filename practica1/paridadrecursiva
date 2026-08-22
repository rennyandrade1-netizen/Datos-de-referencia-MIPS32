.data
	msj1: .asciiz "Ingrese un Numero: "
	salto: .asciiz "\n"
	msj2: .asciiz "El resultado es: "
.text

	main:
	
	la $a0, msj1
	li $v0, 4
	syscall
	
	li $v0, 5
	syscall
	move $a0, $v0
	
	jal paridad
	move $t0, $v0
	
	la $a0, salto
	li $v0, 4
	syscall
	
	la $a0, msj2
	li $v0, 4
	syscall
	
	move $a0, $t0
    	li $v0, 1
    	syscall

    	la $a0, salto
    	li $v0, 4
    	syscall

    	li $v0, 10
    	syscall
	
	#Funcion Recursiva de Paridad
		
	paridad:
	addi $sp, $sp, -8
	sw $ra, 4($sp)
	sw $a0, 0($sp)
		
	caso_base:
	beq $a0, $zero, val_cero
		
	sig_iteracion:
	subi $a0, $a0, 1
	jal paridad
		
	operacion:
	li $t0, 1
	sub $v0, $t0, $v0
	j recursividad
		
	val_cero:
	move $v0, $zero
		
	recursividad:
	lw $ra, 4($sp)
	addi $sp, $sp, 8
	jr $ra
		
