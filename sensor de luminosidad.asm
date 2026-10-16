.data

LUZ_CONTROL: .word 0x10000000
LUZ_ESTADO:  .word 0x10000004
LUZ_DATOS:   .word 0x10000008

.text
.globl InicializarSensorLuz
.globl LeerLuminosidad

main:
    
	jal InicializarSensorLuz
    	jal LeerLuminosidad
    
	move $s0, $v0
	move $s1, $v1
    
	li   $v0, 1        
	move $a0, $s0      
 	syscall            
    
	li $v0, 10
	syscall

InicializarSensorLuz:
	addi $sp, $sp, -4
	sw $ra, 0($sp)

	la   $t0, LUZ_CONTROL
	lw   $t1, 0($t0)           
    
 #-------------Medicion------------------- 
   
	la   $t0, LUZ_ESTADO
	lw   $t2, 0($t0)      
	
#-----------A partir de este punto haz los cambios manuales----------        

	li   $t3, 1
	sw   $t3, 0($t1)

esperar_sensor:

	lw   $t4, 0($t2)          
    
	li   $t5, 1
	beq  $t4, $t5, sensor_listo
    
	j    esperar_sensor

sensor_listo:
	lw   $ra, 0($sp)
	addi $sp, $sp, 4
	jr   $ra


LeerLuminosidad:
	addi $sp, $sp, -4
	sw $ra, 0($sp)

	la   $t0, LUZ_ESTADO
	lw   $t1, 0($t0)         
    
#-------------Medicion------------------- 
    
	la   $t0, LUZ_DATOS
	lw   $t2, 0($t0)     
	
#-----------A partir de este punto haz los cambios manuales----------      

	lw   $t3, 0($t1)          
	li   $t4, 1
	bne  $t3, $t4, error_lectura

	lw   $v0, 0($t2)        
	li   $v1, 0               
	j    fin_leer

error_lectura:
	li   $v0, 0               
	li   $v1, -1              

fin_leer:
	lw   $ra, 0($sp)
	addi $sp, $sp, 4
	jr   $ra
