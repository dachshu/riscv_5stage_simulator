#include "instruction.h"
#include "consts.h"
#include <iostream>

using namespace std;

Opcode int_to_opcode(uint32_t insn) {
	uint32_t opcode = insn & OPCODE_MASK;

	switch (opcode)
	{
	case 0b0110111: return Opcode::LUI;
	case 0b0010111: return Opcode::AUIPC;
	case 0b1101111: return Opcode::JAL;
	case 0b1100111: return Opcode::JALR;
	case 0b1100011: return Opcode::BRANCH;
	case 0b0000011: return Opcode::LOAD;
	case 0b0100011: return Opcode::STORE;
	case 0b0010011: return Opcode::OP_IMM;
	case 0b0110011: return Opcode::OP;
	case 0b1110011: return Opcode::SYSTEM;
	case 0b0000111: return Opcode::LOAD_FP;
	case 0b0100111: return Opcode::STORE_FP;
	case 0b1010011: return Opcode::OP_FP;
	case 0b0001111: return Opcode::FENCE;
	case 0b0101111: return Opcode::AMO;
	default: {
		clog << "Unknown opcde " << hex << opcode << endl;
		exit(1);
        break;
	}
	}
}


Type opcode_to_type(Opcode opcode) {
	switch (opcode)
	{
	case Opcode::LUI: return Type::U;
	case Opcode::AUIPC: return Type::U;
	case Opcode::JAL: return Type::J;
	case Opcode::JALR: return Type::I;
	case Opcode::BRANCH: return Type::B;
	case Opcode::LOAD: return Type::I;
	case Opcode::STORE: return Type::S;
	case Opcode::OP_IMM: return Type::I;
	case Opcode::OP: return Type::R;
	case Opcode::SYSTEM: return Type::I;
	case Opcode::LOAD_FP: return Type::I;
	case Opcode::STORE_FP: return Type::S;
	case Opcode::OP_FP: return Type::R;
	case Opcode::FENCE: return Type::I;
	case Opcode::AMO: return Type::R;
	}
}

bool is_shift(Fields& fields) {
	if (fields.opcode == 0x13 // OpImm
		&& (fields.funct3 == 0x1 || fields.funct3 == 0x5)) // shift
		return true;
	return false;
}

void parse_type_i(uint32_t insn, Fields &fields){

	fields.opcode = insn & OPCODE_MASK;
	fields.rd = (insn & RD_MASK) >> RD_SHIFT;
	fields.funct3 = (insn & FUNCT3_MASK) >> FUNCT3_SHIFT;
	fields.rs1 = (insn & RS1_MASK) >> RS1_SHIFT;

	if (is_shift(fields)) {
		// Shift: insn[24:20] -> shamt (imm)
		fields.imm = (insn & RS2_MASK) >> RS2_SHIFT;
		// virtual value
		fields.funct7 = (insn & FUNCT7_MASK) >> FUNCT7_SHIFT;
	}
	else {
		// Arithmetic, logical, load, or jalr: insn[31:20] -> imm[11:0]
		fields.imm = (insn & 0xfff00000) >> 20;
	}
}

void parse_type_r(uint32_t insn, Fields& fields) {
	fields.opcode = insn & OPCODE_MASK;
	fields.funct3 = (insn & FUNCT3_MASK) >> FUNCT3_SHIFT;
	fields.funct7 = (insn & FUNCT7_MASK) >> FUNCT7_SHIFT;
	fields.rs1 = (insn & RS1_MASK) >> RS1_SHIFT;
	fields.rs2 = (insn & RS2_MASK) >> RS2_SHIFT;
	fields.rd = (insn & RD_MASK) >> RD_SHIFT;
}

void parse_type_s(uint32_t insn, Fields& fields) {
	fields.opcode = insn & OPCODE_MASK;
	fields.funct3 = (insn & FUNCT3_MASK) >> FUNCT3_SHIFT;
	fields.rs1 = (insn & RS1_MASK) >> RS1_SHIFT;
	fields.rs2 = (insn & RS2_MASK) >> RS2_SHIFT;
	// insn[31:25] -> imm[11:5]
	uint32_t imm_high = (insn & 0xfe000000) >> 20;
	// insn[11:7] -> imm[4:0]
	uint32_t imm_low = (insn & 0xf80) >> 7;
	fields.imm = imm_high | imm_low;
}

void parse_type_b(uint32_t insn, Fields& fields) {
	fields.opcode = insn & OPCODE_MASK;
	fields.funct3 = (insn & FUNCT3_MASK) >> FUNCT3_SHIFT;
	fields.rs1 = (insn & RS1_MASK) >> RS1_SHIFT;
	fields.rs2 = (insn & RS2_MASK) >> RS2_SHIFT;
	// insn[7] -> imm[11]
	uint32_t imm_bit_11 = (insn & 0x80) << 4;
	// insn[31] -> imm[12]
	uint32_t imm_bit_12 = (insn & 0x80000000) >> 19;
	// insn[30:25] -> imm[10:5]
	uint32_t imm_high = (insn & 0x7e000000) >> 20;
	// insn[11:8] -> imm[4:1]
	uint32_t imm_low = (insn & 0xf00) >> 7;
	fields.imm = (imm_bit_12 | imm_bit_11 | imm_high | imm_low);
}

void parse_type_u(uint32_t insn, Fields& fields) {
	fields.opcode = insn & OPCODE_MASK;
	fields.rd = (insn & RD_MASK) >> RD_SHIFT;
	// insn[31:12] -> imm[31:12]
	fields.imm = insn & 0xfffff000;
}

void parse_type_j(uint32_t insn, Fields& fields) {
	fields.opcode = insn & OPCODE_MASK;
	fields.rd = (insn & RD_MASK) >> RD_SHIFT;
	// insn[31] -> imm[20]
	uint32_t imm_bit_20 = (insn & 0x80000000) >> 11;
	// insn[30:21] -> imm[10:1]
	uint32_t imm_low = (insn & 0x7fe00000) >> 20;
	// insn[20] -> imm[11]
	uint32_t imm_bit_11 = (insn & 0x100000) >> 9;
	// isns[19:12] -> imm[19:12]
	uint32_t imm_high = insn & 0xff000;
	fields.imm = (imm_bit_20 | imm_high | imm_bit_11 | imm_low);
}

Function insn_to_fn(Opcode opcode, uint32_t funct3, uint32_t funct7, uint32_t imm) {
	switch (opcode)
	{
	case Opcode::LUI: return Function::LUI;
	case Opcode::AUIPC: return Function::AUIPC;
	case Opcode::JAL: return Function::JAL;
	case Opcode::JALR: return Function::JALR;
	case Opcode::LOAD_FP: return Function::FLW;
	case Opcode::STORE_FP: return Function::FSW;
	default:
		break;
	}

	if (opcode == Opcode::BRANCH) {
		switch (funct3)
		{
		case 0b000: return Function::BEQ;
		case 0b001: return Function::BNE;
		case 0b100: return Function::BLT;
		case 0b101: return Function::BGE;
		case 0b110: return Function::BLTU;
		case 0b111: return Function::BGEU;
        }
	}

	if (opcode == Opcode::LOAD) {
		switch (funct3)
		{
		case 0b000: return Function::LB;
		case 0b001: return Function::LH;
		case 0b010: return Function::LW;
		case 0b100: return Function::LBU;
		case 0b101: return Function::LHU;
		}
	}

	if (opcode == Opcode::STORE) {
		switch (funct3)
		{
		case 0b000: return Function::SB;
		case 0b001: return Function::SH;
		case 0b010: return Function::SW;
		}
	}

	if (opcode == Opcode::OP_IMM) {
		switch (funct3)
		{
				case 0b000: return Function::ADDI;
				case 0b010: return Function::SLTI;
				case 0b011: return Function::SLTIU;
				case 0b100: return Function::XORI;
				case 0b110: return Function::ORI;
				case 0b111: return Function::ANDI;
				case 0b001: return Function::SLLI;
				case 0b101: {
					if (funct7 == 0b0000000) return Function::SRLI;
					if (funct7 == 0b0100000) return Function::SRAI;
					break;
				};		
		}
		
	}

	if (opcode == Opcode::OP) {
		switch (funct7) {
		case 0b0000000: {
			switch (funct3) {
			case 0b000: return Function::ADD;
			case 0b001: return Function::SLL;
			case 0b010: return Function::SLT;
			case 0b011: return Function::SLTU;
			case 0b100: return Function::XOR;
			case 0b101: return Function::SRL;
			case 0b110: return Function::OR;
			case 0b111: return Function::AND;
			}
			break;
		}
		case 0b0100000: {
			if (funct3 == 0b000) return Function::SUB;
			if (funct3 == 0b101) return Function::SRA;
			break;
		}
		case 0b0000001: {
			switch (funct3) {
			case 0b000:return Function::MUL;
			case 0b001:return Function::MULH;
			case 0b010:return Function::MULHSU;
			case 0b011:return Function::MULHU;
			case 0b100:return Function::DIV;
			case 0b101:return Function::DIVU;
			case 0b110:return Function::REM;
			case 0b111:return Function::REMU;
			}
			break;
		}
		}
	}

	if (opcode == Opcode::OP_FP) {
		switch (funct7) {
		case 0b0000000: return Function::FADD_S;
		case 0b0000100: return Function::FSUB_S;
		case 0b0001000: return Function::FMUL_S;
		case 0b0001100: return Function::FDIV_S;
        default:
                        std::clog << "not defined insn" << std::endl;
                        exit(1);
        }
	}

	if (opcode == Opcode::SYSTEM) {
		if (imm == 0) return Function::ECALL;
		if (imm == 1) return Function::EBREAK;
	}

	if (opcode == Opcode::FENCE) {
		if (funct3 == 0b000) return Function::FENCE;
		if (funct3 == 0b001) return Function::FENCE_I;
	}

	if (opcode == Opcode::AMO) {
		funct7 = funct7 >> 2;
		switch (funct7)
		{
		case 0b00010: return Function::LR_W;
		case 0b00011: return Function::SC_W;
		case 0b00001: return Function::AMOSWAP_W;
		case 0b00000: return Function::AMOADD_W;
		case 0b00100: return Function::AMOXOR_W;
		case 0b01100: return Function::AMOAND_W;
		case 0b01000: return Function::AMOOR_W;
		case 0b10000: return Function::AMOMIN_W;
		case 0b10100: return Function::AMOMAX_W;
		case 0b11000: return Function::AMOMINU_W;
		case 0b11100: return Function::AMOMAXU_W;
		}
	}

	clog << "Failed to decode instruction !!";
	exit(1);
    return Function::ADDI;
}

// sign extend immediate value
int32_t gen_immediate(Opcode opcode, uint32_t imm) {
	uint32_t shamt = 0;
	switch (opcode) {
	case Opcode::LUI:
	case Opcode::AUIPC:
		shamt = 0;
		break;
	case Opcode::JAL:
	case Opcode::JALR:
		shamt = 12;
		break;
	case Opcode::BRANCH:
		shamt = 19;
		break;
	default:
		shamt = 20;
		break;
	}

	if (imm) {
		int32_t v = int32_t(imm);
		return ((v << shamt) >> shamt);
	}

	return 0;
}


void Instruction::decode()
{
	if (value == 0) return;
    
    if (value == 0x003027f3 || value == 0x00351073)
        value = 0x13;
	
    opcode = int_to_opcode(value);
	type = opcode_to_type(opcode);
	switch (type) {
			case Type::R: 
				parse_type_r(value, fields);
				break;
			case Type::I: 
				parse_type_i(value, fields);
				break;
			case Type::S: 
				parse_type_s(value, fields);
				break;
			case Type::B:
				parse_type_b(value, fields);
				break;
			case Type::U:
				parse_type_u(value, fields);
				break;
			case Type::J:
				parse_type_j(value, fields);
				break;
	}										  
	fields.imm = uint32_t(gen_immediate(opcode, fields.imm));
	function = insn_to_fn(opcode, fields.funct3, fields.funct7, fields.imm);
}

