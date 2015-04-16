.ifndef __DAM_H__
.define __DAM_H__

.define   ROM_START         $D000

.define   VIDEO_START       $F000
.define   VIDEO_COMMAND     $F3E8
.define   VIDEO_ARG         $F3E9
.define   KEY_HIT_ADDR      $F401
.define   KEY_CODE_ADDR     $F402

.macro    send_char   ch
    lda   ch
    sta   VIDEO_ARG

    lda   #$00
    sta   VIDEO_COMMAND
.endmacro

.macro    clear_screen
    lda   0
    sta   VIDEO_ARG

    lda   #$02
    sta   VIDEO_COMMAND
.endmacro

.endif
