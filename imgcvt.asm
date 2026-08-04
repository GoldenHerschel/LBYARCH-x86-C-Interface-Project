;Christian Sanidad, Adolfo Heruelo

section .data
    ; 1/255 stored as a single precision constant so we can multiply instead of divide
    ; divide is way slower than multiply so we bake the reciprocal in here
    inv255 dd 0.00392156862745098

section .text
    global imgCvtGrayInttoFloat

imgCvtGrayInttoFloat:
    mov     r10, r8                 ; r10 holds the input pointer
    mov     r11, r9                 ; r11 holds the output pointer
    mov     eax, ecx                ; eax gets height
    imul    eax, edx                ; times width so eax is the total pixel count

    mov     ecx, eax                ; ecx is our loop limit from here on
    test    ecx, ecx
    jle     done                    ; nothing to do on an empty image

    movss   xmm1, [rel inv255]      ; scalar SIMD load of the 1/255 constant
    xor     r9d, r9d                ; r9 is the pixel index, start at zero

loop_start:
    movzx   eax, byte [r10 + r9]    ; grab one uint8 pixel and zero extend it
    xorps   xmm0, xmm0              ; clear xmm0 first to dodge the false dependency
    cvtsi2ss xmm0, eax              ; scalar SIMD int to float convert
    mulss   xmm0, xmm1              ; scalar SIMD multiply by 1/255
    movss   [r11 + r9*4], xmm0      ; scalar SIMD store, floats are 4 bytes wide
    inc     r9                      ; next pixel
    cmp     r9d, ecx
    jb      loop_start              ; keep going until we hit the pixel count

done:
    ret