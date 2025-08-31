/*
    MIT License

    Copyright (c) 2025 Rachit Dhar

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long DWORD;
typedef unsigned char BYTE;

/*
TASM TURING MACHINE & LANGUAGE IMPLEMENTATION
*********************************************

TAPE:
    An array of units acting as a universal memory.
    Divided into

    (1) STORAGE : To store data required by the program (like variables)
    (2) STACK : To keep track of the call stack
    (2) DISPLAY : To store data that can be displayed as output
    (3) INSTRUCTION MEMORY : To store the program itself

TASM INSTRUCTION SET:

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
*/

/*
MEMORY ADDRESSES
****************

STORAGE MEMORY (for storage of data during program execution):
    0 (MEM) to 99999 (MEM_END)

    The first 5 addresses are PRIVILEDGED REGISTERS (TEMP, ZF, CF, DISP, and STK)
    used internally for executing certain TASM instructions

STACK MEMORY (to store the call stack of the program):
    100000 (STACK_END) to 100999 (STACK)

    The stack grows backwards in direction, so as to not overflow into
    display memory by accident

DISPLAY MEMORY (for storage of data that will be displayed after program completion):
    101000 (OUT) to 200999 (OUT_END)

INSTRUCTION MEMORY (for storing the program instructions itself):
    201000 (MAIN) to 300999 (END)
*/

#define STORE_SIZE 100000       // program storage memory size
#define STACK_SIZE 1000         // call stack memory size
#define DISPLAY_SIZE 100000     // text output capacity
#define INSTR_SIZE 100000       // instruction memory size

#define _MEM 0             // start of memory
#define _MEM_END 99999     // max position of memory
#define _STACK_END 100000  // end of the stack memory
#define _STACK 100999      // start point of the stack memory
#define _OUT 101000        // start point of display
#define _OUT_END 200999    // max position of display
#define _MAIN 201000       // entry point of the program
#define _END 300999        // max position for instructions

#define _TEMP 0          // register : temp storage
#define _ZF 1            // register : zero flag
#define _CF 2            // register : carry flag
#define _DISP 3          // register : lowest free display address
#define _STK 4           // register : highest free stack position address (i.e. <STACK_TOP_ADDR> - 1)
#define _SAFE_MEM 5      // start of useable memory

// Simple <char*, DWORD> Hash Map implementation
// (primarily needed for label to address mapping in the assembler)

struct Pair {
    char *key;
    DWORD value;
    struct Pair *next;
};
typedef struct Pair Pair;

void init_map(Pair **map)
{
    for (int i = 0; i < STACK_SIZE; i++)
    map[i] = NULL;
}

// a very basic hash function
DWORD hash(const char *key)
{
    DWORD h = 0;
    while (*key) {
	h = (h * 31) + (*key);
	key++;
    }
    return h % STACK_SIZE;
}

void map_insert(Pair **map, const char *key, DWORD value)
{
    DWORD index = hash(key);

    // check if key already exists
    Pair *curr = map[index];
    while (curr != NULL) {
	if (strcmp(curr->key, key) == 0) {
	    curr->value = value;
	    return;
	}
	curr = curr->next;
    }

    Pair *p = malloc(sizeof(Pair));
    p->key = strdup(key);
    p->value = value;
    p->next = map[index];
    map[index] = p;
}

DWORD *map_get(Pair **map, const char *key)
{
    DWORD index = hash(key);

    Pair *curr = map[index];
    while (curr != NULL) {
	if (strcmp(curr->key, key) == 0) return &(curr->value);
	curr = curr->next;
    }
    return NULL;
}

/*
TURING MACHINE INSTRUCTION SET
******************************
*/

typedef enum {
    I_NONE, // 0x0 | do nothing (the default state)
    I_HALT, // 0x1 | to stop the program execution

    /* Standard instructions */
    I_JUMP, // 0x2 | to move the tape pointer to the address stored at the current position
    I_CMP,  // 0x3 |compare the values at _ptr.data and position (and set flags accordingly)
    I_JE,   // 0x4 | to jump to the address if equal
    I_JNE,  // 0x5 | to jump to the address if not equal
    I_JG,   // 0x6 | to jump to the address if greater
    I_JGE,  // 0x7 | to jump to the address if greater or equal
    I_JL,   // 0x8 | to jump to the address if less
    I_JLE,  // 0x9 | to jump to the address if less or equal
    I_READ, // 0xA | read data (to _ptr) from the address stored at the current position
    I_WRITE,// 0xB | write data (from _ptr) to the address stored at the current position
    I_CALL, // 0xC | call a particular instruction block (jump to a label)
    I_RET,  // 0XD | return to the caller

    /* Bitwise instructions */
    I_AND,    // 0xE  | bitwise AND
    I_OR,     // 0xF  | bitwise OR
    I_XOR,    // 0x10 | bitwise XOR
    I_NOT,    // 0x11 | bitwise NOT
    I_LSHIFT, // 0x12 | left shift
    I_RSHIFT, // 0x13 | right shift

    /* Arithmetic instructions */
    I_ADD, // 0x14 | (current position data + _ptr.data) -> current position
    I_SUB, // 0x15 | (current position data - _ptr.data) -> current position
    I_MUL, // 0x16 | (current position data * _ptr.data) -> current position
    I_DIV, // 0x17 | (current position data / _ptr.data) -> current position

    /* I/O instructions */
    I_OUT, // 0x18 | prints the data in display memory (upto the first NULL character)
} INSTRUCTION;

/*
DATA TYPES
**********

In this architecture, we are defining 2 kinds of types:

    T_UINT (Unsigned Integer)
    T_CHAR (Character)

We shall control the type simply by using a dtype variable in the BLOCK:

    0 --> T_UINT
    1 --> T_CHAR

(NOTE FOR CONTEXT: In TASM v1, we used actual data types in the instructions
which were put and moved around with the help of the I_STEAL and I_BURN
instructions. This, however, turns out to be a very bad idea, because
such instructions literally alter the instructions at runtime, which became
obvious when the fib.tasm example program I wrote failed at runtime due
to the T_UINT and T_CHAR instructions being written into the instruction
memory, despite being non-instructions. Even in general, something that
is fundamentally not an instruction [like T_UINT and T_CHAR] shouldn't
be stored into instruction variables)
*/

/*
TAPE BLOCK
**********
*/

typedef struct {
    INSTRUCTION ins;
    DWORD data; // 4 bytes of data
    BYTE dtype;
} BLOCK;

/*
TAPE POINTER
************
*/

typedef struct {
    DWORD pos;
    DWORD data;
    BYTE dtype;
} TAPE_PTR;

static TAPE_PTR _ptr;

/*
IMPLEMENTATION BEGINS HERE
**************************
*/

static BLOCK tape[STORE_SIZE + STACK_SIZE + DISPLAY_SIZE + INSTR_SIZE];
int memdump = 0; // whether to generate memory dump files after execution is complete

// to load instructions that read the value stored at an address, and pass it into the upcoming instruction
// (overwrite_at: the number of steps ahead to overwrite at)
void load_deref_instructions(DWORD addr, int overwrite_at)
{
    tape[_ptr.pos].ins = I_READ;
    tape[_ptr.pos].data = addr;
    _ptr.pos++;

    tape[_ptr.pos].ins = I_WRITE;
    tape[_ptr.pos].data = _ptr.pos + overwrite_at; // to overwrite the next position
    _ptr.pos++;
}

// map the tasm instruction to turing machine instruction(s) and load them
//
// if deref_1 is 1, then instructions will be added before each instruction containing a1, to overwrite it
// with the dereferenced address. the same goes for deref_2.
void load_instruction(const char *ins, DWORD a1, DWORD a2, BYTE data_type, int deref_1, int deref_2)
{
    /* 0 operand instructions */
    if (strcmp(ins, "hlt") == 0) {
	tape[_ptr.pos].ins = I_HALT;
	tape[_ptr.pos].data = 0;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "out") == 0) {
	tape[_ptr.pos].ins = I_OUT;
	tape[_ptr.pos].data = 0;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "ret") == 0) {
	tape[_ptr.pos].ins = I_RET;
	tape[_ptr.pos].data = 0;
	_ptr.pos++;
	return;
    }

    /* 1 operand instructions */
    if (strcmp(ins, "not") == 0) {
	if (deref_1) load_deref_instructions(a1, 1);

	tape[_ptr.pos].ins = I_NOT;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "jmp") == 0) {
	if (deref_1) load_deref_instructions(a1, 1);

	tape[_ptr.pos].ins = I_JUMP;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "call") == 0) {
	if (deref_1) load_deref_instructions(a1, 1);

	tape[_ptr.pos].ins = I_CALL;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "je") == 0) {
	if (deref_1) load_deref_instructions(a1, 1);

	tape[_ptr.pos].ins = I_JE;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "jne") == 0) {
	if (deref_1) load_deref_instructions(a1, 1);

	tape[_ptr.pos].ins = I_JNE;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "jg") == 0) {
	if (deref_1) load_deref_instructions(a1, 1);

	tape[_ptr.pos].ins = I_JG;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "jge") == 0) {
	if (deref_1) load_deref_instructions(a1, 1);

	tape[_ptr.pos].ins = I_JGE;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "jl") == 0) {
	if (deref_1) load_deref_instructions(a1, 1);

	tape[_ptr.pos].ins = I_JL;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "jle") == 0) {
	if (deref_1) load_deref_instructions(a1, 1);

	tape[_ptr.pos].ins = I_JLE;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    /* 2 operand instructions */
    if (strcmp(ins, "cmp") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_CMP;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "put") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 3);

	tape[_ptr.pos].ins = I_NONE;
	tape[_ptr.pos].data = a2;
	tape[_ptr.pos].dtype = data_type;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = _ptr.pos - 1;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_WRITE;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "mov") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_WRITE;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "and") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_AND;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "or") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_OR;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "xor") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_XOR;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "lsh") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_LSHIFT;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "rsh") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_RSHIFT;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "add") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_ADD;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "sub") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_SUB;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "mul") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_MUL;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }

    if (strcmp(ins, "div") == 0) {
	if (deref_2) load_deref_instructions(a2, deref_1 ? 3 : 1);
	if (deref_1) load_deref_instructions(a1, 2);

	tape[_ptr.pos].ins = I_READ;
	tape[_ptr.pos].data = a2;
	_ptr.pos++;

	tape[_ptr.pos].ins = I_DIV;
	tape[_ptr.pos].data = a1;
	_ptr.pos++;
	return;
    }
}

// create three files displaying the entire memory contents
void generate_memory_dump()
{
    // write store file
    FILE *store_file = fopen("__STORE_DUMP.tasm.txt", "w");
    if (store_file == NULL) {
	fprintf(stderr, "ERROR: Failed to create store dump file");
	exit(1);
    }

    // write line by line from store memory
    for (DWORD i = _MEM; i <= _MEM_END; i++) {
	fprintf(store_file, "0x%08lx [_MEM + %010lu] \t0x%08lx  0x%08lx  %u\n", i, i - _MEM, tape[i].ins, tape[i].data, tape[i].dtype);
    }
    fclose(store_file);

    // write display file
    FILE *display_file = fopen("__DISPLAY_DUMP.tasm.txt", "w");
    if (store_file == NULL) {
	fprintf(stderr, "ERROR: Failed to create display dump file");
	exit(1);
    }

    // write line by line from display memory
    for (DWORD i = _OUT; i <= _OUT_END; i++) {
	fprintf(display_file, "0x%08lx [_OUT + %010lu] \t0x%08lx  0x%08lx  %u\n", i, i - _OUT, tape[i].ins, tape[i].data, tape[i].dtype);
    }
    fclose(display_file);

    // write instruction file
    FILE *ins_file = fopen("__INSTRUCTION_DUMP.tasm.txt", "w");
    if (ins_file == NULL) {
	fprintf(stderr, "ERROR: Failed to create instruction dump file");
	exit(1);
    }

    // write line by line from instruction memory
    for (DWORD i = _MAIN; i <= _END; i++) {
	fprintf(ins_file, "0x%08lx [_MAIN + %010lu] \t0x%08lx  0x%08lx  %u\n", i, i - _MAIN, tape[i].ins, tape[i].data, tape[i].dtype);
    }
    fclose(ins_file);
}


// ASSEMBLER
//
// read, parse, and load the tasm program into instruction memory
void assemble_tasm(const char *tasm_file_name)
{
    FILE *tasm_file = fopen(tasm_file_name, "r");
    if (!tasm_file) {
	fprintf(stderr, "ERROR: .tasm file not found");
	exit(1);
    }

    _ptr.pos = _MAIN;
    char line[256];

    Pair *label_to_address_map[STACK_SIZE];
    init_map(label_to_address_map);

    // load line by line
    int line_num = 0;

    while (fgets(line, sizeof(line), tasm_file) != NULL) {
	line_num++;

	char *comment_start = strstr(line, "//");
	if (comment_start != NULL) {
            *comment_start = '\0';  // terminate string at the start of the comment
	}

	char ins[100] = "", first[100] = "", second[200] = "";

	sscanf(line, "%s %s %[^\n]", ins, first, second);

	size_t ins_len = strlen(ins);
	if (ins_len == 0) continue;

	// check for instruction memory overflow
	if (_ptr.pos > _END) {
	    fprintf(stderr, "ERROR: Memory overflow occurred [Line %d]. Instruction memory limit exceeded.", line_num);
	    if (memdump) generate_memory_dump();
	    exit(1);
	}

	// for labels
	if (ins[ins_len - 1] == ':') {
	    char *label = malloc(sizeof(char) * ins_len);
	    strncpy(label, ins, ins_len - 1); // remove the ':' from the label
	    label[ins_len - 1] = '\0';

	    if (map_get(label_to_address_map, label) != NULL) {
		fprintf(stderr, "ERROR: Duplicate label definitions encountered [Line %d]", line_num);
		exit(1);
	    }

	    map_insert(label_to_address_map, label, _ptr.pos);
	    continue;
	}

	DWORD a1, a2;
	BYTE data_type = 0;
	int deref_1 = 0, deref_2 = 0;
	size_t first_len = strlen(first);

	if (first[0] == '0' && first[1] == 'x') {
	    a1 = strtoul(first, NULL, 16);
	} else if (first[0] == '[' && first[1] == '0' && first[2] == 'x' && first[first_len - 1] == ']') {
	    // mark the first address for dereferencing
	    deref_1 = 1;

	    char *addr_contained = malloc(first_len - 1);
	    strncpy(addr_contained, first + 1, first_len - 2); // get the number without the square brackets
	    addr_contained[first_len - 2] = '\0';

	    a1 = strtoul(addr_contained, NULL, 16);
	} else if (first_len > 0) {
	    // label handling (for the case of "call" instruction)
	    DWORD *retrieved_addr = map_get(label_to_address_map, first);
	    if (retrieved_addr == NULL) {
		fprintf(stderr, "ERROR: Undefined label encountered [Line %d]", line_num);
		exit(1);
	    }

	    a1 = *retrieved_addr;
	}

        size_t len = strlen(second);

	if (second[0] == '"') { // for char / string data
            if (len > 1 && second[len - 1] == '"') second[len - 1] = '\0';

	    for (int i = 1; i < len - 1; i++) {
		a2 = (DWORD)(second[i]);
		data_type = 1;

		load_instruction(ins, a1, a2, data_type, deref_1, deref_2);
		a1++;
	    }
	    continue;
	} else if (second[0] == '[' && second[len - 1] == ']') { // for unsigned int address enclosed in []
	    // mark the second address for dereferencing
	    deref_2 = 1;

	    char *addr_contained = malloc(len - 1);
	    strncpy(addr_contained, second + 1, len - 2); // get the number without the square brackets
	    addr_contained[len - 2] = '\0';

	    a2 = strtoul(addr_contained, NULL, 0);
	} else if (len > 0) { // for unsigned int data (hex / oct / dec)
            a2 = strtoul(second, NULL, 0);
	}

	load_instruction(ins, a1, a2, data_type, deref_1, deref_2);
    }
    // add halt at the end for safety
    tape[_ptr.pos].ins = I_HALT;
    tape[_ptr.pos].data = 0;

    // set the pointer position to the main label
    DWORD *main_addr = map_get(label_to_address_map, "main");
    if (main_addr == NULL) {
	fprintf(stderr, "ERROR: Could not find \"main\"");
	exit(1);
    }
    _ptr.pos = *main_addr;

    // set the initial addresses in the flags
    tape[_DISP].data = _OUT;
    tape[_STK].data = _STACK;

    fclose(tasm_file);
}

// output display text
void output()
{
    DWORD final_addr = _ptr.pos + 1;
    _ptr.pos = _OUT;
    int is_escaped = 0;

    while (_ptr.pos < _OUT_END && _ptr.pos < tape[_DISP].data) {
	DWORD val = tape[_ptr.pos].data;

	// handle escape sequences
	if (is_escaped) {
	    if (val == (DWORD)'n') putchar('\n');
	    else if (val == (DWORD)'r') putchar('\r');

	    is_escaped = 0;
	    _ptr.pos++;
	    continue;
	}

	// if dtype is 1, treat like a char
	if (tape[_ptr.pos].dtype) {
	    if (val == (DWORD)'\\') {
		is_escaped = 1;
		_ptr.pos++;
		continue;
	    }
	    putchar((char) (val & 0xFF));
	} else printf("%lu", val);

	is_escaped = 0;
	_ptr.pos++;
    }
    _ptr.pos = final_addr;
}

// run the program on the turing machine (tape)
// (contains all instruction implementations)
void run()
{
    int is_halted = 0;

    while (!is_halted)
    {
	if (_ptr.pos > _END) {
	    fprintf(stderr, "RUNTIME ERROR: Memory out of bounds. Address 0x%lx [%lu] does not exist", _ptr.pos, _ptr.pos);
	    if (memdump) generate_memory_dump();
	    exit(1);
	}

	DWORD addr = tape[_ptr.pos].data;
	if (addr > _END) {
	    fprintf(stderr, "RUNTIME ERROR: Memory out of bounds. Address 0x%lx [%lu] does not exist", addr, addr);
	    if (memdump) generate_memory_dump();
	    exit(1);
	}

	// execute the instruction
	switch (tape[_ptr.pos].ins) {
	case I_NONE:
	    _ptr.pos++;
	    break;
	case I_HALT:
	    is_halted = 1;
	    break;
	case I_JUMP:
	    _ptr.pos = addr;
	    break;
	case I_CMP:
	    tape[_ZF].data = tape[addr].data == _ptr.data;
	    tape[_CF].data = tape[addr].data < _ptr.data;
	    _ptr.pos++;
	    break;
	case I_JE:
	    _ptr.pos = tape[_ZF].data == 1 ? addr : _ptr.pos + 1;
	    break;
	case I_JNE:
	    _ptr.pos = tape[_ZF].data == 0 ? addr : _ptr.pos + 1;
	    break;
	case I_JG:
	    _ptr.pos = (tape[_ZF].data == 0 && tape[_CF].data == 0) ? addr : _ptr.pos + 1;
	    break;
	case I_JGE:
	    _ptr.pos = tape[_CF].data == 0 ? addr : _ptr.pos + 1;
	    break;
	case I_JL:
	    _ptr.pos = tape[_CF].data == 1 ? addr : _ptr.pos + 1;
	    break;
	case I_JLE:
	    _ptr.pos = (tape[_ZF].data == 1 || tape[_CF].data == 1) ? addr : _ptr.pos + 1;
	    break;
	case I_READ:
	    _ptr.data = tape[addr].data;
	    _ptr.dtype = tape[addr].dtype;
	    _ptr.pos++;
	    break;
	case I_WRITE:
	    tape[addr].data = _ptr.data;
	    tape[addr].dtype = _ptr.dtype;

	    if (addr >= tape[_DISP].data && addr <= _OUT_END) tape[_DISP].data = addr + 1;
	    _ptr.pos++;
	    break;
	case I_AND:
	    tape[addr].data &= _ptr.data;
	    _ptr.pos++;
	    break;
	case I_OR:
	    tape[addr].data |= _ptr.data;
	    _ptr.pos++;
	    break;
	case I_XOR:
	    tape[addr].data ^= _ptr.data;
	    _ptr.pos++;
	    break;
	case I_NOT:
	    tape[addr].data = !tape[addr].data;
	    _ptr.pos++;
	    break;
	case I_LSHIFT:
	    tape[addr].data <<= _ptr.data;
	    _ptr.pos++;
	    break;
	case I_RSHIFT:
	    tape[addr].data >>= _ptr.data;
	    _ptr.pos++;
	    break;
	case I_ADD:
	    tape[addr].data += _ptr.data;
	    _ptr.pos++;
	    break;
	case I_SUB:
	    tape[addr].data -= _ptr.data;
	    _ptr.pos++;
	    break;
	case I_MUL:
	    tape[addr].data *= _ptr.data;
	    _ptr.pos++;
	    break;
	case I_DIV:
	    tape[addr].data /= _ptr.data;
	    _ptr.pos++;
	    break;
	case I_OUT:
	    output();
	    break;
	case I_CALL:
	    if (tape[_STK].data < _STACK_END) {
		fprintf(stderr, "RUNTIME ERROR: Stack overflow occurred. Execution terminated.");
		if (memdump) generate_memory_dump();
		exit(1);
	    }
	    tape[tape[_STK].data].data = _ptr.pos + 1;
	    tape[_STK].data--;
	    _ptr.pos = addr;
	    break;
	case I_RET:
	    tape[_STK].data++;
	    _ptr.pos = tape[tape[_STK].data].data;
	    break;
	default:
	    fprintf(stderr, "RUNTIME ERROR: Invalid instruction :: %u", tape[_ptr.pos].ins);
	    if (memdump) generate_memory_dump();
	    exit(1);
	}
    }
}

// check the extension of a file (ext is to be passed without a dot)
int has_extension(const char *file_name, const char *ext) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) return 0;
    return strcmp(dot + 1, ext) == 0;
}

void main(int argc, char** argv)
{
    if (argc < 2 || !has_extension(argv[1], "tasm")) {
	fprintf(stderr, "ERROR: Provide the .tasm file name in the argument");
	exit(1);
    }

    // flag for memory dump files to be generated after execution in complete
    memdump = (argc > 2 && strcmp(argv[2], "-memdump") == 0);

    assemble_tasm(argv[1]);
    run();

    if (memdump) generate_memory_dump();
}
