.data
    msg_verde:      .asciiz "Semaforo en verde, esperando pulsador \n"
    msg_pulsador:   .asciiz "Pulsador activado: en 20 segundos el semaforo cambiara a amarillo \n"
    msg_amarillo:   .asciiz "Semaforo en amarillo, en 10 segundos el semaforo cambiara a rojo \n"
    msg_rojo:       .asciiz "Semaforo en rojo, en 30 segundos el semaforo cambiara a verde \n"

.text
.globl main

main:
    # Cargar direcciones MMIO del teclado
    li   $s0, 0xffff0000      # Control del teclado
    li   $s1, 0xffff0004      # Datos del teclado

ciclo_semaforo:
    # ESTADO VERDE
    li   $v0, 4
    la   $a0, msg_verde
    syscall

    # Esperar tecla 's' (polling del teclado MMIO)
espera_s:
    lw   $t0, 0($s0)          # Leer control
    andi $t0, $t0, 1          # Bit 0 = ready
    beqz $t0, espera_s        # No hay tecla, seguir esperando

    lw   $t1, 0($s1)          # Leer tecla
    li   $t2, 's'
    bne  $t1, $t2, espera_s   # Si no es 's', descartar y seguir esperando

    # Pulsador activado: 20 segundos -> amarillo
    li   $v0, 4
    la   $a0, msg_pulsador
    syscall

    li   $a0, 20
    jal  sleep_seconds

    # ESTADO AMARILLO
    li   $v0, 4
    la   $a0, msg_amarillo
    syscall

    li   $a0, 10
    jal  sleep_seconds

    # ESTADO ROJO
    li   $v0, 4
    la   $a0, msg_rojo
    syscall

    li   $a0, 30
    jal  sleep_seconds

    j    ciclo_semaforo       

sleep_seconds:
    move $t0, $a0            
    
sleep_loop:
    beqz $t0, sleep_done      

    li   $v0, 32
    li   $a0, 1000            
    syscall

    subi $t0, $t0, 1          
    j    sleep_loop

sleep_done:
    jr   $ra
