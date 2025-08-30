# TASM (Turing Assembly)

Turing Assembly (TASM): An assembly language that runs on an in-built virtual Turing-like Machine.

The idea of this project originated with the notion of a Turing Machine as a model for a universal
computer. I was curious about the simplicity of implementing a TM, which in its traditional sense
is only an array ("tape") that contains symbols, along with a pointer head that can read and write
to the cells and move around. My implementation deviates from this basic model in two main ways:

- The tape is an array of structs, each with an instruction and data variable.
- The set of instructions has been extended beyond what is considered the "bare minimum" theoretically needed, for the sake of practicality.
- The tape pointer by default just moves towards the right.

Furthermore, the tape is divided into 4 regions of memory for their respective functions:

- STORAGE
- STACK
- DISPLAY
- INSTRUCTIONS

## Usage

To assemble (and run) a .tasm program file:

```
tasm <FILE_NAME>
```

If you want to also retrieve the memory contents of the file after it has been executed,
you can run it with the "-memdump" flag:

```
tasm <FILE_NAME> -memdump
```

This can sometimes be helpful for debugging purposes. To see how the dump files look, you
can go look at [memdump__powers_of_two](./examples/memdump__powers_of_two)

## The Language (TASM)

I have created a simple assembly language called TASM, that can be loaded onto the tape, and run.
The tasm.c file handles all details of both the turing machine and the TASM assembler.

The assembler simply reads the .tasm file given, translates each instruction into a set of
instructions for the turing machine, and loads them into the instruction memory. Then the machine
is set to run.

## Basic Syntax

```
<INSTRUCTION>	<DESTINATION_ADDRESS>	<SOURCE_ADDRESS> | <VALUE>
```

For example:

```asm
put		0x4 	"Hello World!\n"
```

## Instruction Set

The instruction set is given below. It is relatively similar to most standard assembly instructions.

```
	put <ADDR> <DATA>         set data to addr                    (put)
    mov <ADDR1> <ADDR2>       move data from 2 to 1               (move)
    cmp <ADDR1> <ADDR2>       sets _ZF and _CF as per (1 - 2)     (compare)
    jmp <ADDR>                move ptr to addr                    (jump)
    je  <ADDR>                move ptr to addr if _ZF=1           (jump if equal)
    jne <ADDR>                move ptr to addr if _ZF=0           (jump if not equal)
    jg  <ADDR>                move ptr to addr if _ZF=0 && _CF=0  (jump if >)
    jge <ADDR>                move ptr to addr if _CF=0           (jump if >=)
    jl  <ADDR>                move ptr to addr if _CF=1           (jump if <)
    jle <ADDR>                move ptr to addr if _ZF=1 || _CF=1  (jump if <=)
    call <ADDR>               add to stack, and go to address     (call)
    and <ADDR1> <ADDR2>       set (1 & 2) to 1                    (bit and)
    or  <ADDR1> <ADDR2>       set (1 | 2) to 1                    (bit or)
    xor <ADDR1> <ADDR2>       set (1 ^ 2) to 1                    (bit xor)
    not <ADDR>                set ~addr to addr                   (bit not)
    lsh <ADDR1> <ADDR2>       set (1.data << 2.data) to 1         (left shift)
    rsh <ADDR1> <ADDR2>       set (1.data >> 2.data) to 1         (right shift)
    add <ADDR1> <ADDR2>       set (1 + 2) to 1                    (add)
    sub <ADDR1> <ADDR2>       set (1 - 2) to 1                    (subtract)
    mul <ADDR1> <ADDR2>       set (1 * 2) to 1                    (multiply)
    div <ADDR1> <ADDR2>       set (1 / 2) to 1                    (divide)
    ret                       move ptr to address in stack top    (return)
    out                       display output                      (output)
    hlt                       end program execution               (halt)
```

## Special Memory Addresses

The following memory addresses are unique (the values are defined in tasm.c):

- _MEM / _TEMP
- _ZF
- _CF
- _DISP
- _STK
- _MEM_END
- _OUT
- _OUT_END
- _MAIN
- _END
