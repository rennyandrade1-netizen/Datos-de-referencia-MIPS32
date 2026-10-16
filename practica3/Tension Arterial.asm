.data
# Direcciones simuladas de los registros de tensión arterial
TENSION_CONTROL: .word 0x10000010
TENSION_ESTADO:  .word 0x10000014
TENSION_SISTOL:  .word 0x10000018
TENSION_DIASTOL: .word 0x1000001C

.text
.globl controlador_tension

.text
.globl main

main:

	jal controlador_tension
    
	move $s0, $v0
	move $s1, $v1    

	li   $v0, 1         
	move $a0, $s0       
	syscall             

	li   $v0, 1        
	move $a0, $s1       
	syscall             
 
	li $v0, 10
	syscall

controlador_tension:
	addi $sp, $sp, -4
	sw $ra, 0($sp)

	la   $t0, TENSION_CONTROL
	lw   $t1, 0($t0)

#-------------Medicion-------------------           

	la   $t0, TENSION_ESTADO
	lw   $t2, 0($t0)          

    	la   $t0, TENSION_SISTOL
    	lw   $t3, 0($t0)          

    	la   $t0, TENSION_DIASTOL
    	lw   $t4, 0($t0)
    	
#-----------A partir de este punto haz los cambios manuales----------       

    	li   $t5, 1
    	sw   $t5, 0($t1)

esperar_medicion:
 
    	lw   $t6, 0($t2)          
    	beq  $t6, $zero, esperar_medicion  

    	lw   $v0, 0($t3)           
    	lw   $v1, 0($t4)           

    	lw   $ra, 0($sp)
    	addi $sp, $sp, 4
    	jr   $ra
