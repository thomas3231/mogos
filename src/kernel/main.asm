bits 16
global start


%define ENDL 0x0D, 0x0A

start:
    mov si, msg_hello
    call puts

    call enable_a20
    call switch_to_pm

    jmp $

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

gdt_start:
    dq 0x0000000000000000

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x02
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x02
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start + 0x20000

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

global idt_load
idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret

extern keyboard_handler_c

global keyboard_handler_asm
keyboard_handler_asm:
    pusha
    call keyboard_handler_c
    mov al, 0x20
    out 0x20, al        ; tell PIC "handled"
    popa
    iretd

init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x70000        ; 0x90000 - 0x20000, so the real linear address is still 0x90000
    mov esp, ebp

    extern kernel_main
    call kernel_main

    jmp $

[bits 16]
msg_hello: db 'Hello world from KERNEL!', ENDL, 0
