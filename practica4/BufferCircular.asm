.data
    # Buffer circular de 128 bytes
    buffer:         .space 128
    buffer_tama:    .word 128
    escri_ptr:      .word 0
    read_ptr:       .word 0
    conta:          .word 0

    # tecladod
    KB_CONTROL:     .word 0xFFFF0000
    KB_DATA:        .word 0xFFFF0004

    
    msg_inicio:      .asciiz "=== Iniciando captura de teclado (20 segundos). Solo mayusculas A-Z ===\n"
    msg_fianal:        .asciiz "\n--- Tiempo terminado. Contenido del buffer: ---\n"
    msg_vacio:      .asciiz "(Buffer vacio)\n"
    msg_de_separa:  .asciiz "\n--- Reiniciando... ---\n\n"

.text
.globl main

main:
    # Cargar direcciones MMIO
    lw $s6, KB_CONTROL      # $s6 = control del teclado
    lw $s7, KB_DATA         # $s7 = datos del teclado

main_loop:
    # Reiniciar buffer
    sw $zero, escri_ptr
    sw $zero, read_ptr
    sw $zero, conta

    # Mensaje de inicio
    li $v0, 4
    la $a0, msg_inicio
    syscall

    #tiempo inicial
    li $v0, 30
    syscall
    move $s0, $a0           # $s0 = tiempo inicial
    li $s1, 20000           # $s1 = 20 segundos 

capture_loop:
    # Verificar tiempo transcurrido
    li $v0, 30
    syscall
    sub $t0, $a0, $s0       # $t0 = tiempo transcurrido
    bge $t0, $s1, fianal_capture

    # verificamos si hay teclas precionadas 
    lw $t0, 0($s6)          # Leer registro de control
    andi $t0, $t0, 0x0001   
    beq $t0, $zero, capture_loop  # No hay tecla, seguir

    # Leer carácter
    lw $t0, 0($s7)          # $t0 = carácter ASCII

    # Filtro de ,mayusculas 
    blt $t0, 0x41, capture_loop   # Si < 'A', descartar
    bgt $t0, 0x5A, capture_loop   # Si > 'Z', descartar

    # Guardar en buffer 
    la $t1, buffer
    lw $t2, escri_ptr
    add $t3, $t1, $t2       # Dirección de escritura
    sb $t0, 0($t3)          # Guardar byte

    # actualizacion del pinteto
    addi $t2, $t2, 1
    li $t3, 128
    rem $t2, $t2, $t3
    sw $t2, escri_ptr

    # Incrementar conta 
    lw $t2, conta
    addi $t2, $t2, 1
    ble $t2, $t3, save_conta
    li $t2, 128             # para no exceder el tamaño
    
save_conta:
    sw $t2, conta

    j capture_loop

fianal_capture:
 
    li $v0, 4
    la $a0, msg_fianal
    syscall

    # Imprimir contenido del buffer
    lw $t0, conta
    beq $t0, $zero, print_vacio

    
    lw $s2, conta           # $s2 = caracteres restantes
    lw $s3, read_ptr        # $s3 = posición de lectura
    la $s4, buffer          # $s4 = base del buffer
    li $s5, 128             # $s5 = tamaño

print_conte:

    beq $s2, $zero, final

    add $t0, $s4, $s3
    lb $a0, 0($t0)          # Cargar carácter
    li $v0, 11              # Print char
    syscall

    # Avanzar read_ptr
    addi $s3, $s3, 1
    rem $s3, $s3, $s5

    subi $s2, $s2, 1
    j print_conte

print_vacio:
    li $v0, 4
    la $a0, msg_vacio
    syscall

final:
    li $v0, 4
    la $a0, msg_de_separa
    syscall

    j main_loop             # Repetir indefinidamente
