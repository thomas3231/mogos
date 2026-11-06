bits 16
global start
global framebuffer_addr
global screen_width
global screen_height
global screen_pitch
global screen_bpp

screen_width: dw 0
screen_height: dw 0
screen_pitch: dw 0
screen_bpp: db 0

%define ENDL 0x0D, 0x0A

start:
    mov si, msg_hello
    call puts

    call enable_a20
    call set_vesa_mode

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

print_hex16:
    push ax
    push cx
    mov cx, 4
.loop:
    rol ax, 4
    push ax
    and al, 0x0F
    cmp al, 10
    jl .digit
    add al, 'A' - 10
    jmp .store
.digit:
    add al, '0'
.store:
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    pop ax
    loop .loop
    pop cx
    pop ax
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



set_vesa_mode:
    ; Keep the existing QEMU mode as the first choice.
    push es
    mov ax, ds
    mov es, ax
    mov ax, 0x4F01
    mov cx, 0x118
    mov di, vbe_mode_info
    int 0x10
    pop es
    cmp ax, 0x004F
    jne .scan_modes

    mov ax, 0x4F02
    mov bx, 0x4118
    int 0x10
    cmp ax, 0x004F
    jne .scan_modes

    jmp .save_mode

.scan_modes:
    mov dword [vbe_info], 'VBE2'
    push es
    mov ax, ds
    mov es, ax
    mov ax, 0x4F00
    mov di, vbe_info
    int 0x10
    pop es
    cmp ax, 0x004F
    jne .vesa_failed

    mov byte [vbe_pass], 0
.start_pass:
    mov ax, [vbe_info + 14]
    mov [mode_off], ax
    mov ax, [vbe_info + 16]
    mov [mode_seg], ax

.next_mode:
    mov ax, [mode_seg]
    mov fs, ax
    mov si, [mode_off]
    mov cx, [fs:si]
    add word [mode_off], 2
    jnc .pointer_ready
    add word [mode_seg], 0x1000
.pointer_ready:
    cmp cx, 0xFFFF
    je .end_of_list
    mov [candidate_mode], cx

    push es
    mov ax, ds
    mov es, ax
    mov ax, 0x4F01
    mov di, vbe_mode_info
    int 0x10
    pop es
    cmp ax, 0x004F
    jne .next_mode

    mov ax, [vbe_mode_info]
    and ax, 0x0099
    cmp ax, 0x0099
    jne .next_mode
    cmp byte [vbe_mode_info + 27], 6
    jne .next_mode
    mov al, [vbe_mode_info + 25]
    cmp al, 24
    je .bpp_ok
    cmp al, 32
    jne .next_mode
.bpp_ok:
    mov ax, [vbe_mode_info + 18]
    cmp byte [vbe_pass], 0
    jne .loose
    cmp ax, 1024
    jne .next_mode
    cmp word [vbe_mode_info + 20], 768
    jne .next_mode
    jmp .try_set
.loose:
    cmp ax, 800
    jb .next_mode
    cmp ax, 1280
    ja .next_mode

.try_set:
    movzx eax, word [vbe_mode_info + 16]
    movzx edx, word [vbe_mode_info + 20]
    imul eax, edx
    cmp eax, 0x800000
    jae .next_mode

    mov bx, [candidate_mode]
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    je .save_mode
    jmp .next_mode

.end_of_list:
    cmp byte [vbe_pass], 0
    jne .vesa_failed
    mov byte [vbe_pass], 1
    jmp .start_pass

.save_mode:
    mov eax, [vbe_mode_info + 40]
    mov [framebuffer_addr], eax

    mov ax, [vbe_mode_info + 16]
    mov [screen_pitch], ax

    mov ax, [vbe_mode_info + 18]
    mov [screen_width], ax

    mov ax, [vbe_mode_info + 20]
    mov [screen_height], ax

    mov al, [vbe_mode_info + 25]
    mov [screen_bpp], al

    ret

.vesa_failed:
    mov si, msg_vesa_failed
    call puts
    jmp $

vbe_mode_info: times 256 db 0
vbe_info: times 512 db 0
mode_off: dw 0
mode_seg: dw 0
candidate_mode: dw 0
vbe_pass: db 0
framebuffer_addr: dd 0


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

global fast_copy
fast_copy:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov edi, [ebp + 8]      ; destination pointer (1st arg)
    mov esi, [ebp + 12]     ; source pointer (2nd arg)
    mov ecx, [ebp + 16]     ; number of DWORDs to copy (3rd arg)

    rep movsd

    pop edi
    pop esi
    pop ebp
    ret

global task_context_switch
task_context_switch:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]
    mov [eax], esp
    mov esp, [esp + 24]

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret


%macro EXCEPTION_NOERR 1
global exception_stub_%1
exception_stub_%1:
    push dword %1
    call exception_common
%endmacro

%macro EXCEPTION_ERR 1
global exception_stub_%1
exception_stub_%1:
    ; CPU already pushed an error code for this one -- don't push our own
    push dword %1
    call exception_common
%endmacro

extern exception_handler_c

exception_common:
    pusha
    mov eax, [esp + 36]     ; retrieve the vector number we pushed
    push eax
    call exception_handler_c
    ; no return -- exception_handler_c halts permanently
    add esp, 4
    popa
    add esp, 4
    iretd

EXCEPTION_NOERR 0
EXCEPTION_NOERR 1
EXCEPTION_NOERR 2
EXCEPTION_NOERR 3
EXCEPTION_NOERR 4
EXCEPTION_NOERR 5
EXCEPTION_NOERR 6
EXCEPTION_NOERR 7
EXCEPTION_ERR   8
EXCEPTION_ERR   13
EXCEPTION_ERR   14


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

extern timer_handler_c

global timer_handler_asm
timer_handler_asm:
    pusha
    call timer_handler_c
    mov al, 0x20
    out 0x20, al
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
msg_vesa_failed: db 'Failed vesa connection!', ENDL, 0
msg_width:   db 'Width: ', 0
msg_height:  db ENDL, 'Height: ', 0
msg_pitch:   db ENDL, 'Pitch: ', 0
msg_bpp:     db ENDL, 'BPP: ', 0
msg_newline: db ENDL, 0
msg_fb: db ENDL, 'Framebuffer: ', 0