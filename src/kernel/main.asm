org 0x0
bits 16

%define ENDL 0x0D, 0x0A

start:
    mov si, msg_hello
    call puts

    call enable_a20
    call switch_to_pm

    jmp $                          ; should never actually reach here

; ---- real mode routines ----

puts:
    push si
    push ax
    push bx
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    pop si
    ret

enable_a20:
    in al, 0x92
    test al, 2
    jnz .after
    or al, 2
    and al, 0xFE
    out 0x92, al
.after:
    ret

; ---- GDT ----

gdt_start:
    dq 0x0000000000000000

gdt_code:
    dw 0xFFFF                  ; limit low
    dw 0x0000                  ; base low
    db 0x02                    ; base mid  (kernel loaded at 0x20000)
    db 10011010b               ; access
    db 11001111b               ; flags + limit high
    db 0x00                    ; base high

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x02                    ; base mid  (kernel loaded at 0x20000)
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start + 0x20000     ; <-- the fix: physical address, not file offset

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

switch_to_pm:
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:init_pm

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000
    mov esp, ebp

    call BEGIN_PM

BEGIN_PM:
    ; clear the entire screen first (80x25 characters, 2 bytes each = 4000 bytes)
    mov edi, 0x98000            ; = 0xB8000 - 0x20000 (DS base compensation)
    mov ecx, 80*25               ; total character cells
    mov ax, 0x0F20                ; attribute 0x0F (white on black) + space character (0x20)
.clear_loop:
    mov [edi], ax
    add edi, 2
    loop .clear_loop

    ; now print our message
    mov esi, msg_pm
    mov edi, 0x98000
    mov ah, 0x0F
.loop:
    lodsb
    or al, al
    jz .done
    mov [edi], al
    mov [edi+1], ah
    add edi, 2
    jmp .loop
.done:
    jmp $

[bits 16]
msg_hello: db 'Hello world from KERNEL!', ENDL, 0
msg_pm:    db 'Successfully entered 32-bit protected mode!', 0