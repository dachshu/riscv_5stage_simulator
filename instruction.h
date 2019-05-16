#pragma 

#include <stdint.h>

enum class Opcode {
	LUI,
	AUIPC,
	JAL,
	JALR,
	BRANCH,
	LOAD,
	STORE,
	OP_IMM,
	OP,
	SYSTEM,
	LOAD_FP,
	STORE_FP,
	OP_FP,

	FENCE,
	AMO
};

enum class Type {
	R,
	I,
	S,
	B,
	U,
	J
};

enum class Function {
	LUI,        // load upper immediate
	AUIPC,      // add upper immediate to PC

	JAL,        // jump and link
	JALR,       // jump and link register

	// BRANCH
	BEQ,        // branch if equal
	BNE,        // branch if not equal
	BLT,        // branch if less than
	BGE,        // branch if greater or equal
	BLTU,       // branch if less than (unsigned)
	BGEU,       // branch if greater of equal (unsigned)

	// LOAD
	LB,         // load byte
	LH,         // load halfword
	LW,         // load word
	LBU,        // load byte (unsigned)
	LHU,        // load halfword (unsigned)

	// STORE
	SB,         // store byte
	SH,         // store halfword
	SW,         // store word

	// OP-IMM
	ADDI,       // Add immediate
	SLTI,       // set less than immediate
	SLTIU,      // set less than immediate (unsigned)
	XORI,       // exclusive or immediate
	ORI,        // logical or immediate
	ANDI,       // logical and immediate
	SLLI,       // shift left logical immediate
	SRLI,       // shift right logical immediate
	SRAI,       // shift right arithmetic immediate

	// OP (operations on registers)
	ADD,        // add
	SUB,        // subtract
	SLL,        // shift left logical
	SLT,        // set less than
	SLTU,       // set less than (unsigned)
	XOR,        // exclusive or
	SRL,        // shift right logical
	SRA,        // shirft right arithmetic
	OR,         // logical or
	AND,        // logical and
	// OP (MULDIV)
	MUL,        // XLEN bit * XLEN bit multiplication -> lower XLEN bits in rd
	MULH,       // perform same but return upper XLEN bits of 2*XLEN bit prodoct (signed * signed)
	MULHSU,     // (signed * unsigned)
	MULHU,      // (unsigned * unsigned)
	DIV,        // division of XLEN bits by XLEN bits (signed)
	DIVU,       // (unsigned)
	REM,        // remainder of division operation (signed)
	REMU,       // (unsigned)

	// SYSTEM
	ECALL,      // system call
	EBREAK,		// for debug

	// ATOMIC
	LR_W,
	SC_W,
	AMOSWAP_W,
	AMOADD_W,
	AMOXOR_W,
	AMOAND_W,
	AMOOR_W,
	AMOMIN_W,
	AMOMAX_W,
	AMOMINU_W,
	AMOMAXU_W,

	FLW,        // load single-precision floating-point value from memory int floating point register
	FSW,        // store single-precision value from floating-point register

	// OP-FP
	FADD_S,
	FSUB_S,
	FMUL_S,
	FDIV_S,

	// FENCE
	FENCE,
	FENCE_I,
	NOP

	//FMADD_S,    // rs1*rs2 + rs3
	//FMSUB_S,    // rs1*rs2 - rs3
	//FNMSUB_S,   // -rs1*rs2 + rs3
	//FNM11ADD_S,   // -rs1*rs2 - rs3
};

struct Fields {
	uint32_t rs1{ 0 };
	uint32_t rs2{ 0 };
	uint32_t rd{ 0 };
	uint32_t imm{ 0 };
	uint32_t funct3{ 0 };
	uint32_t funct7{ 0 };
	uint32_t opcode{ 0 };
	uint32_t pc{ 0 };
};

class Instruction {
public:
	uint32_t value;
	Opcode opcode{ Opcode::OP_IMM };
	Type type{ Type::I };
	Fields fields;
	Function function{ Function::ADDI };
	

	Instruction(uint32_t v) : value(v) {}

	void decode();
};
