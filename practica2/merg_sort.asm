.data
    array:      .word   38, 27, 43, 3, 9, 82, 10
    n:          .word   7
    temp:       .space  100               
    msg_orig:   .asciiz "Arreglo original: "
    msg_sorted: .asciiz "Arreglo ordenado: "
    space_char: .asciiz " "
    newline:    .asciiz "\n"

.text
.globl main

main:
    
    li      $v0, 4
    la      $a0, msg_orig
    syscall

    la      $a0, array
    lw      $a1, n
    jal     imprimir_arreglo

    la      $a0, array          
    li      $a1, 0              
    lw      $a2, n              
    addi    $a2, $a2, -1        
    jal     mergesort

    li      $v0, 4
    la      $a0, msg_sorted
    syscall

    la      $a0, array
    lw      $a1, n
    jal     imprimir_arreglo

    li      $v0, 10
    syscall

mergesort:

    bge     $a1, $a2, fin_mergesort

    addi    $sp, $sp, -20
    sw      $ra, 16($sp)
    sw      $s0, 12($sp)       
    sw      $a0, 8($sp)
    sw      $a1, 4($sp)
    sw      $a2, 0($sp)

    add     $s0, $a1, $a2
    sra     $s0, $s0, 1         

    move    $a2, $s0            
    jal     mergesort

    lw      $a0, 8($sp)
    lw      $a1, 4($sp)
    lw      $a2, 0($sp)

    addi    $a1, $s0, 1         
    jal     mergesort

    lw      $a0, 8($sp)
    lw      $a1, 4($sp)
    lw      $a2, 0($sp)

    move    $a3, $s0            
    jal     merge

    lw      $ra, 16($sp)
    lw      $s0, 12($sp)
    addi    $sp, $sp, 20

fin_mergesort:
    jr      $ra

merge:
    la      $t6, temp           
    move    $t0, $a1            
    addi    $t1, $a3, 1         
    move    $t2, $a1            

loop_comparar:
    bgt     $t0, $a3, copiar_der_resto   
    bgt     $t1, $a2, copiar_izq_resto   

    sll     $t3, $t0, 2
    add     $t3, $a0, $t3
    lw      $t4, 0($t3)

    sll     $t3, $t1, 2
    add     $t3, $a0, $t3
    lw      $t5, 0($t3)

    ble     $t4, $t5, menor_izq

menor_der:

    sll     $t3, $t2, 2
    add     $t3, $t6, $t3
    sw      $t5, 0($t3)
    addi    $t1, $t1, 1
    addi    $t2, $t2, 1
    j       loop_comparar

menor_izq:

    sll     $t3, $t2, 2
    add     $t3, $t6, $t3
    sw      $t4, 0($t3)
    addi    $t0, $t0, 1
    addi    $t2, $t2, 1
    j       loop_comparar

copiar_izq_resto:
    bgt     $t0, $a3, copiar_a_original
    sll     $t3, $t0, 2
    add     $t3, $a0, $t3
    lw      $t4, 0($t3)         

    sll     $t3, $t2, 2
    add     $t3, $t6, $t3
    sw      $t4, 0($t3)         

    addi    $t0, $t0, 1
    addi    $t2, $t2, 1
    j       copiar_izq_resto

copiar_der_resto:
    bgt     $t1, $a2, copiar_a_original
    sll     $t3, $t1, 2
    add     $t3, $a0, $t3
    lw      $t5, 0($t3)        

    sll     $t3, $t2, 2
    add     $t3, $t6, $t3
    sw      $t5, 0($t3)        

    addi    $t1, $t1, 1
    addi    $t2, $t2, 1
    j       copiar_der_resto

copiar_a_original:

    move    $t0, $a1            
loop_copia:
    bgt     $t0, $a2, fin_merge

    sll     $t3, $t0, 2
    add     $t4, $t6, $t3       
    lw      $t5, 0($t4)         

    add     $t4, $a0, $t3      
    sw      $t5, 0($t4)         

    addi    $t0, $t0, 1
    j       loop_copia

fin_merge:
    jr      $ra

imprimir_arreglo:
    move    $t0, $a0            
    li      $t1, 0              

loop_imprimir:
    bge     $t1, $a1, fin_imprimir

    lw      $a0, 0($t0)         
    li      $v0, 1              
    syscall

    la      $a0, space_char     
    li      $v0, 4
    syscall

    addi    $t0, $t0, 4     
    addi    $t1, $t1, 1         
    j       loop_imprimir

fin_imprimir:
    la      $a0, newline
    li      $v0, 4
    syscall
    jr      $ra
