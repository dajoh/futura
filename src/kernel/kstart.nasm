; Multiboot constants
MB_FLAG_MODALIGN equ  1 << 0
MB_FLAG_MEMINFO  equ  1 << 1
MB_MAGIC         equ  0x1BADB002
MB_FLAGS         equ  MB_FLAG_MODALIGN | MB_FLAG_MEMINFO
MB_CHECKSUM      equ -(MB_MAGIC + MB_FLAGS)

; Multiboot section
[section .multiboot]
[extern __kernel_beg]
[extern __kernel_end]
[global kboot]
kboot:
    ; Build two page tables (2048 entries)
    mov esi, 0
    mov edi, page_table0
    mov ecx, 2048
.loop:
    mov [edi], esi
    or dword [edi], 3
    add esi, 4096
    add edi, 4
    loop .loop
.done:
	; Map the page tables to both virtual addresses 0x00000000 and 0xC0000000
	mov dword [page_dir + 0 * 4], (page_table0 + 0x003)
	mov dword [page_dir + 1 * 4], (page_table1 + 0x003)
	mov dword [page_dir + 768 * 4], (page_table0 + 0x003)
	mov dword [page_dir + 769 * 4], (page_table1 + 0x003)

	; Set cr3 to the address of the page_dir
	mov ecx, page_dir
	mov cr3, ecx

	; Enable paging and write-protect
	mov ecx, cr0
	or ecx, 0x80010000
	mov cr0, ecx

	; Jump to higher half
	lea ecx, [kstart]
	jmp ecx

; Multiboot header
align 8
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

; Initial page directories
align 4096
page_dir:
    times 4096 db 0
page_table0:
    times 4096 db 0
page_table1:
    times 4096 db 0

; Stack
[section .bss]
[global __kernel_stack_beg]
[global __kernel_stack_end]
align 16
__kernel_stack_beg:
    resb 16*1024
__kernel_stack_end:

; IDT, GDT
[section .data]
[global __kernel_idt_beg]
[global __kernel_gdt_beg]
align 8 ; The base addresses of the IDT should be aligned on an 8-byte boundary to maximize performance of cache line fills.
__kernel_idt_beg:
    times 256 dq 0
__kernel_idt_ptr:
    dw 2047
    dd __kernel_idt_beg
align 8 ; The base address of the GDT should be aligned on an eight-byte boundary to yield the best processor performance.
__kernel_gdt_beg:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF ; Kernel code segment
    dq 0x00CF92000000FFFF ; Kernel data segment
    dq 0x00CFFA000000FFFF ; User code segment
    dq 0x00CFF2000000FFFF ; User data segment
    dq 0x0000000000000000 ; TSS
__kernel_gdt_ptr:
    dw 47
    dd __kernel_gdt_beg

; Entry point
[section .text]
[global kstart]
[extern kinit]
kstart:
    ; Set up stack and save multiboot parameters
    mov esp, __kernel_stack_end
    push ebx
    push eax

    ; Set up GDT
    lgdt [__kernel_gdt_ptr]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush_cs
.flush_cs:

    ; Set up IDT
    lidt [__kernel_idt_ptr]

    ; Disable VGA blink
        ; Read I/O Address 0x03DA to reset index/data flip-flop
        mov dx, 0x03DA
        in al, dx
        ; Write index 0x30 to 0x03C0 to set register index to 0x30
        mov dx, 0x03C0
        mov al, 0x30
        out dx, al
        ; Read from 0x03C1 to get register contents
        inc dx
        in al, dx
        ; Unset Bit 3 to disable Blink
        and al, 0xF7
        ; Write to 0x03C0 to update register with changed value
        dec dx
        out dx, al

    ; Call kinit
    cld
    call kinit

    ; Halt
.hang:
    hlt
    jmp .hang

; ----------------------------------
; kflushtss
; ----------------------------------
[global kflushtss]
kflushtss:
    mov ax, 0x2B
    ltr ax
    ret

; ----------------------------------
; kenterusermode
; ----------------------------------
[global kenterusermode]
kenterusermode:
    cli
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push 0x23
    push eax
    pushf
    push 0x1b
    push dword [esp + 20]
    iret

; ----------------------------------
; SchSwitchTask
; ----------------------------------
struc SchTask
    .next: resd 1
    .id:   resd 1
    .name: resd 1
    .esp:  resd 1
    .sleepNext:  resd 1
    .sleepUntil: resq 1
endstruc
[extern SchCurrentTask]
[global SchSwitchTask]
SchSwitchTask:
    ; Save callee saved regs (sysv abi)
    push ebx
    push esi
    push edi
    push ebp

    ; Save current task stack
    mov edi, [SchCurrentTask]
    mov [edi + SchTask.esp], esp

    ; Switch current task to target task
    mov esi, [esp + 20]          ; Get target task from args to this function
    mov [SchCurrentTask], esi    ; Set current task ptr
    mov esp, [esi + SchTask.esp] ; Swap out the stack (!!!)
    nop                          ; Is this needed?
    nop                          ; Is this needed?

    ; Restore callee saved regs (sysv abi)
    pop ebp
    pop edi
    pop esi
    pop ebx

    ; Re-enable IRQs. We need to do this because this function is called from the
    ; IRQ timer handler which will cause the 'iret' in IsrCommonHandler to never be executed.
    sti
    ret

[extern PitCurrentTick]
[extern TscFrequency]
[global TscCalibrate]
TscCalibrate:
    ; Enable interrupts
    sti

    ; Wait until next PIT tick
    mov ebx, [PitCurrentTick]
.wait_until_tick:
    cmp ebx, [PitCurrentTick]
    jz .wait_until_tick

    ; Read TSC
    rdtsc
    mov [TscFrequency + 0], eax
    mov [TscFrequency + 4], edx

    ; Wait 50 PIT ticks (0.5 seconds at 100Hz)
    add ebx, 51
.wait_until_tick2:
    cmp ebx, [PitCurrentTick]
    ja .wait_until_tick2

    ; Read TSC
    rdtsc
    sub eax, [TscFrequency + 0]
    sbb edx, [TscFrequency + 4]

    ; Multiply by 2 to get Hz
    mov ebx, 2
    mul ebx

    ; Save frequency
    mov [TscFrequency + 0], eax
    mov [TscFrequency + 4], edx

    ; Disable interrupts again
    cli
    ret
