.data

    arreglo:  .word 12, 4, 8, 2, 9, 7, 5, 10, 3, 1
    tam:   .word 10
    
    msg_ini:.asciiz "Arreglo original: "
    msg_fin:.asciiz "\nArreglo ordenado: "
    esp:  .asciiz " "
    new_li:.asciiz "\n"

.text


main:

    li $v0, 4
    la $a0, msg_ini
    syscall
    jal print_arreglo

    la $a0, arreglo       
    li $a1, 0           
    la $t0, tam
    lw $t0, 0($t0)      
    addi $a2, $t0, -1   

    jal quicksort

    li $v0, 4
    la $a0, msg_fin
    syscall
    jal print_arreglo
    
    li $v0, 4
    la $a0, new_li
    syscall

    li $v0, 10
    syscall


quicksort:

    bge $a1, $a2, qs_end

    addi $sp, $sp, -20
    sw $ra, 16($sp)      
    sw $a0, 12($sp)      
    sw $a1, 8($sp)       
    sw $a2, 4($sp)       
    sw $s0, 0($sp)       

    jal part
    move $s0, $v0        

    addi $a2, $s0, -1   
    jal quicksort

    lw $a0, 12($sp)      
    addi $a1, $s0, 1     
    lw $a2, 4($sp)       
    jal quicksort

    lw $s0, 0($sp)
    lw $a2, 4($sp)
    lw $a1, 8($sp)
    lw $a0, 12($sp)
    lw $ra, 16($sp)
    addi $sp, $sp, 20

qs_end:
    jr $ra


part:
    
    sll $t0, $a2, 2      
    add $t0, $a0, $t0    
    lw $t1, 0($t0)       

    addi $t2, $a1, -1    
    move $t3, $a1        

part_loop:

    bge $t3, $a2, part_end 

    sll $t4, $t3, 2      
    add $t4, $a0, $t4    
    lw $t5, 0($t4)       

    bge $t5, $t1, part_next

    addi $t2, $t2, 1     

    sll $t6, $t2, 2     
    add $t6, $a0, $t6    
    lw $t7, 0($t6)       
    
    sw $t5, 0($t6)       
    sw $t7, 0($t4)       

part_next:
    addi $t3, $t3, 1     
    j part_loop

part_end:

    addi $t2, $t2, 1     
    sll $t6, $t2, 2      
    add $t6, $a0, $t6    
    
    lw $t7, 0($t6)      
    
    sw $t1, 0($t6)       
    sw $t7, 0($t0)       

    move $v0, $t2        
    jr $ra

print_arreglo:
    la $t0, arreglo        
    la $t1, tam
    lw $t1, 0($t1)      
    li $t2, 0            

print_loop:
    bge $t2, $t1, print_end
    
    lw $a0, 0($t0)
    li $v0, 1
    syscall
    
    li $v0, 4
    la $a0, esp
    syscall
    
    addi $t0, $t0, 4     
    addi $t2, $t2, 1     
    j print_loop

print_end:
    jr $ra
