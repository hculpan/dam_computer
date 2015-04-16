.include "dam.s"

.zeropage
  msg_addr:       .word $0000

.org      ROM_START

.code
START:
    clear_screen

    lda   #<INTRO1
    sta   msg_addr
    lda   #>INTRO1
    sta   msg_addr + 1

    ldy   #$00

msg_loop:
    lda   (msg_addr),y
    beq   next_msg
    iny
    sta   VIDEO_ARG

    lda   #$00
    sta   VIDEO_COMMAND
    jmp   msg_loop

next_msg:
    lda   #<INTRO2
    sta   msg_addr
    lda   #>INTRO2
    sta   msg_addr + 1
    ldy   #$00

next_msg_loop:
    lda   (msg_addr),y
    beq   done
    iny
    sta   VIDEO_ARG

    lda   #$00
    sta   VIDEO_COMMAND
    jmp   next_msg_loop

done:
    jmp   done

.data
  INTRO1:         .byte   "Dual Arduino Mega Machine v0.2", $0A, $00
  INTRO2:         .byte   "Built by Harry Culpan, 2015", $0A, $00
