#pragma once

// Masks to isolate specific parts of the instruction using logical AND (&)
#define FUNCT7_MASK 0xfe000000
#define FUNCT3_MASK 0x7000
#define RS1_MASK 0xf8000
#define RS2_MASK 0x1f00000
#define RD_MASK 0xf80
#define OPCODE_MASK 0x7f
#define BIT30_MASK 0x40000000

// Indices of instruction parts for shifting
#define FUNCT7_SHIFT 25
#define FUNCT3_SHIFT 12
#define RS1_SHIFT 15
#define RS2_SHIFT 20
#define RD_SHIFT 7
#define BIT30_SHIFT 30

#define WORD_SIZE 4
#define HALFWORD_SIZE 2
#define BYTE_SIZE 1

#define FP_ADD_CYCLE 5
#define FP_MUL_CYCLE 10
#define FP_DIV_CYCLE 20
#define CACHE_ACCESS_CYCLE 10
#define MUL_CYCLE 4
#define DIV_CYCLE 8

#define STACK_SIZE 8388608
#define STACK_OFFSET (0x0 - 8388608)
#define REMAIN_SIZE 8388608
#define TLS_SIZE 0xffff


enum class Stage_Result {
	IF, 
	SYSCALL_STALL, 
	BRANCH_STALL, 
	STRUCTURAL, 
	SYSCALL_SYNC_STALL,
	ID, RAW, WAW,
	NOP,
	FPDIV, FPMUL, FPADD,
	MULDIV, EX,
	MEM, WB,
    ISSUE, ADDR, CDB, COMMIT
};

enum class UNIT {
	ALU, MULDIV, FADD, FMUL, FDIV
};
