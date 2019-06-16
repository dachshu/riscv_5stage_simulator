#include "pipeline.h"
#include "syscall.h"
#include <stdio.h>
#include <string>
#include <iostream>

void Pipeline::fetch_first_half()
{
	iF.cond = false;
	iF.syscall_invalidation = false;
	iF.raw_insn = memory->read_int(uint32_t(register_file.pc), WORD_SIZE);
}

Stage_Result Pipeline::fetch_second_half()
{
	if (iF.syscall_invalidation) {
		return Stage_Result::SYSCALL_STALL;
	}

	if (iF.cond) {
		register_file.pc = iF.target_addr;
		return Stage_Result::BRANCH_STALL;
	}

	if (if_id.raw_insn == 0) {
		if_id.pc = register_file.pc;
		if_id.raw_insn = iF.raw_insn;

		register_file.pc += WORD_SIZE;

		return Stage_Result::IF;
	}
	else {
		return Stage_Result::STRUCTURAL;
	}
}

void Pipeline::id_first_half()
{
	id.cond = false;
	id.syscall_invalidation = false;
	id.insn = Instruction{ if_id.raw_insn };
	id.insn.fields.pc = if_id.pc;
	id.insn.decode();
}

bool Pipeline::is_syscall_sync_insn()
{
	if (id_ex_fpadd.insn.value != 0
		|| id_ex_fpmul.insn.value != 0
		|| id_ex_fpdiv.insn.value != 0)
		return true;

	if (ex_mem_alu.insn.value != 0
		&& (ex_mem_alu.insn.opcode == Opcode::LOAD
			|| ex_mem_alu.insn.opcode == Opcode::LOAD_FP
			|| ex_mem_alu.insn.opcode == Opcode::STORE
			|| ex_mem_alu.insn.opcode == Opcode::STORE_FP
			|| ex_mem_alu.insn.opcode == Opcode::AMO))
		return true;

	return false;
}

bool Pipeline::check_gpr_dependency(uint32_t rs)
{
	if (rs == 0) return false;
	if (id_ex_alu.insn.value != 0 && id_ex_alu.insn.fields.rd == rs)
		return true;
	if (ex_mem_alu.insn.value != 0 
		&& (ex_mem_alu.insn.opcode == Opcode::LOAD || ex_mem_alu.insn.opcode == Opcode::AMO)
		&& ex_mem_alu.insn.fields.rd == rs)
		return true;		
    if (rs == 10 && id_ex_alu.insn.value != 0 && id_ex_alu.insn.opcode == Opcode::SYSTEM)
        return true;
    if (rs == 10 && ex_mem_alu.insn.value != 0 && ex_mem_alu.insn.opcode == Opcode::SYSTEM)
        return true;
    if (rs == 10 && mem_wb_alu.insn.value != 0 && mem_wb_alu.insn.opcode == Opcode::SYSTEM)
        return true;

	return false;
}

bool Pipeline::check_fpr_dependency(uint32_t rs)
{
	if (id_ex_fpadd.insn.value != 0 && id_ex_fpadd.insn.fields.rd == rs) return true;
	if (id_ex_fpmul.insn.value != 0 && id_ex_fpmul.insn.fields.rd == rs) return true;
	if (id_ex_fpdiv.insn.value != 0 && id_ex_fpdiv.insn.fields.rd == rs) return true;
	if (ex_mem_alu.insn.value != 0
		&& (ex_mem_alu.insn.opcode == Opcode::LOAD_FP)
		&& ex_mem_alu.insn.fields.rd == rs)
		return true;

	return false;
}


bool Pipeline::check_raw_hazard()
{
	switch (id.insn.opcode)
	{
	case Opcode::LUI: return false;
	case Opcode::AUIPC: return false;
	case Opcode::JAL: return false;
	case Opcode::SYSTEM: return false;
	case Opcode::FENCE: return false;
	case Opcode::JALR:
		return check_gpr_dependency(id.insn.fields.rs1);
	case Opcode::BRANCH:
		return (check_gpr_dependency(id.insn.fields.rs1) 
			|| check_gpr_dependency(id.insn.fields.rs2));
	case Opcode::LOAD:
		return check_gpr_dependency(id.insn.fields.rs1);
	case Opcode::LOAD_FP:
		return check_gpr_dependency(id.insn.fields.rs1);
	case Opcode::STORE:
		return (check_gpr_dependency(id.insn.fields.rs1) 
			|| check_gpr_dependency(id.insn.fields.rs2));
	case Opcode::STORE_FP:
		return (check_gpr_dependency(id.insn.fields.rs1) 
			|| check_fpr_dependency(id.insn.fields.rs2));
	case Opcode::OP_IMM:
		return check_gpr_dependency(id.insn.fields.rs1);
	case Opcode::OP:
		return (check_gpr_dependency(id.insn.fields.rs1) 
			|| check_gpr_dependency(id.insn.fields.rs2));
	case Opcode::OP_FP:
		return (check_fpr_dependency(id.insn.fields.rs1) 
			|| check_fpr_dependency(id.insn.fields.rs2));
	case Opcode::AMO:
		return (check_gpr_dependency(id.insn.fields.rs1)
			|| check_gpr_dependency(id.insn.fields.rs2));
	}
}

bool Pipeline::check_fpr_waw_hazard(uint32_t rd)
{
	if (id_ex_fpadd.insn.value != 0
		&& id_ex_fpadd.insn.fields.rd == rd)
		return true;
	if (id_ex_fpmul.insn.value != 0
		&& id_ex_fpmul.insn.fields.rd == rd)
		return true;
	if (id_ex_fpdiv.insn.value != 0
		&& id_ex_fpdiv.insn.fields.rd == rd)
		return true;
	if (ex_mem_alu.insn.value != 0
		&& ex_mem_alu.insn.opcode == Opcode::LOAD_FP 
		&& ex_mem_alu.insn.fields.rd == rd)
		return true;
	return false;
}


bool Pipeline::check_gpr_waw_hazard(uint32_t rd)
{
	if (rd == 0) return false;
	if (id_ex_alu.insn.value != 0
		&& id_ex_alu.insn.fields.rd == rd)
		return true;
	if (ex_mem_alu.insn.value != 0
		&& (ex_mem_alu.insn.opcode == Opcode::LOAD || ex_mem_alu.insn.opcode == Opcode::AMO)
		&& ex_mem_alu.insn.fields.rd == rd)
		return true;
	return false;
}

bool Pipeline::check_waw_hazard()
{
	switch(id.insn.opcode){
		case Opcode::STORE:
		case Opcode::STORE_FP:
		case Opcode::BRANCH: 
		case Opcode::SYSTEM:
			return false;
		case Opcode::OP_FP:
		case Opcode::LOAD_FP:
			return check_fpr_waw_hazard(id.insn.fields.rd);
		default:
			return check_gpr_waw_hazard(id.insn.fields.rd);
	}
	return false;
}

int32_t Pipeline::fetch_gpr(uint32_t rg)
{
	if (rg == 0) return 0;
	if (ex_mem_alu.insn.value != 0
		&& ex_mem_alu.insn.opcode != Opcode::STORE
		&& ex_mem_alu.insn.opcode != Opcode::STORE_FP
		&& ex_mem_alu.insn.opcode != Opcode::LOAD
		&& ex_mem_alu.insn.opcode != Opcode::LOAD_FP
		&& ex_mem_alu.insn.opcode != Opcode::AMO
		&& ex_mem_alu.insn.fields.rd == rg)
		return ex_mem_alu.alu_result;
	if (ex_mem_muldiv.insn.value != 0
		&& ex_mem_muldiv.insn.fields.rd == rg)
		return ex_mem_muldiv.alu_result;
	if (mem_wb_alu.insn.value != 0
		&& mem_wb_alu.insn.fields.rd == rg
		&& mem_wb_alu.insn.opcode != Opcode::AMO
		&& mem_wb_alu.insn.opcode != Opcode::LOAD_FP
		&& mem_wb_alu.insn.opcode != Opcode::LOAD)
		return mem_wb_alu.alu_result;
	if (mem_wb_alu.insn.value != 0
		&& mem_wb_alu.insn.fields.rd == rg
		&&( mem_wb_alu.insn.opcode == Opcode::LOAD 
			|| mem_wb_alu.insn.opcode == Opcode::AMO))
		return mem_wb_alu.mem_result_i;
	if (mem_wb_muldiv.insn.value != 0
		&& mem_wb_muldiv.insn.fields.rd == rg)
		return mem_wb_muldiv.alu_result;


	return register_file.gpr[rg];
}

float Pipeline::fetch_fpr(uint32_t rg)
{
	if (ex_mem_fpadd.insn.value != 0
		&& ex_mem_fpadd.insn.fields.rd == rg)
		return ex_mem_fpadd.fpu_result;
	if (ex_mem_fpmul.insn.value != 0
		&& ex_mem_fpmul.insn.fields.rd == rg)
		return ex_mem_fpmul.fpu_result;
	if (ex_mem_fpdiv.insn.value != 0
		&& ex_mem_fpdiv.insn.fields.rd == rg)
		return ex_mem_fpdiv.fpu_result;
	if (mem_wb_fpadd.insn.value != 0
		&& mem_wb_fpadd.insn.fields.rd == rg)
		return mem_wb_fpadd.fpu_result;
	if (mem_wb_fpmul.insn.value != 0
		&& mem_wb_fpmul.insn.fields.rd == rg)
		return mem_wb_fpmul.fpu_result;
	if (mem_wb_fpdiv.insn.value != 0
		&& mem_wb_fpdiv.insn.fields.rd == rg)
		return mem_wb_fpdiv.fpu_result;


	if (mem_wb_alu.insn.value != 0
		&& mem_wb_alu.insn.fields.rd == rg
		&& mem_wb_alu.insn.opcode == Opcode::LOAD_FP)
		return mem_wb_alu.mem_result_f;

	return register_file.fpr[rg];
}

void Pipeline::fetch_registers(UNIT unit)
{
	switch (unit)
	{
	case UNIT::ALU: {
		id_ex_alu.pc = if_id.pc;
		id_ex_alu.imm = id.insn.fields.imm;
		id_ex_alu.insn = id.insn;
		id_ex_alu.A = fetch_gpr(id.insn.fields.rs1);
		id_ex_alu.B = fetch_gpr(id.insn.fields.rs2);
		id_ex_alu.Bf = fetch_fpr(id.insn.fields.rs2);
		if (id.insn.opcode == Opcode::JALR) {
			id_ex_alu.target_addr = int32_t(id_ex_alu.imm) + id_ex_alu.A;
			id_ex_alu.target_addr = id_ex_alu.target_addr & 0xfffffffe; // LSB -> 0

		}
		else id_ex_alu.target_addr = if_id.pc + int32_t(id_ex_alu.imm);
		return;
	}
	case UNIT::MULDIV: {
		id_ex_muldiv.insn = id.insn;
		id_ex_muldiv.A = fetch_gpr(id.insn.fields.rs1);
		id_ex_muldiv.B = fetch_gpr(id.insn.fields.rs2);
		return;
	}
	case UNIT::FADD: {
		id_ex_fpadd.insn = id.insn;
		id_ex_fpadd.A = fetch_fpr(id.insn.fields.rs1);
		id_ex_fpadd.B = fetch_fpr(id.insn.fields.rs2);
		return;
	}
	case UNIT::FMUL: {
		id_ex_fpmul.insn = id.insn;
		id_ex_fpmul.A = fetch_fpr(id.insn.fields.rs1);
		id_ex_fpmul.B = fetch_fpr(id.insn.fields.rs2);
		return;
	}
	case UNIT::FDIV: {
		id_ex_fpdiv.insn = id.insn;
		id_ex_fpdiv.A = fetch_fpr(id.insn.fields.rs1);
		id_ex_fpdiv.B = fetch_fpr(id.insn.fields.rs2);
		return;
	}
	}
}

Stage_Result Pipeline::id_second_half()
{
	if (id.insn.value == 0) {
		return Stage_Result::NOP;
	}
	if (id.syscall_invalidation) {
		if_id.raw_insn = 0;
		return Stage_Result::SYSCALL_STALL;
	}

	if(id.cond){
		if_id.raw_insn = 0;
		return Stage_Result::BRANCH_STALL;
	}

	if (id.insn.function == Function::ECALL) {
		if (is_syscall_sync_insn())
			return Stage_Result::SYSCALL_SYNC_STALL;
	}

	bool writable = false;
	UNIT unit = UNIT::ALU;
	switch (id.insn.function)
	{
	case Function::MUL:
	case Function::MULH:
	case Function::MULHSU:
	case Function::MULHU:
	case Function::DIV:
	case Function::DIVU:
	case Function::REM:
	case Function::REMU: {
		if (id_ex_muldiv.insn.value == 0) writable = true;
		unit = UNIT::MULDIV;
		break;
	}
	case Function::FADD_S:
	case Function::FSUB_S:
		if (id_ex_fpadd.insn.value == 0) writable = true;
		unit = UNIT::FADD;
		break;
	case Function::FMUL_S:
		if (id_ex_fpmul.insn.value == 0) writable = true;
		unit = UNIT::FMUL;
		break;
	case Function::FDIV_S:
		if (id_ex_fpdiv.insn.value == 0) writable = true;
		unit = UNIT::FDIV;
		break;
	default:
		if (id_ex_alu.insn.value == 0) writable = true;
		unit = UNIT::ALU;
		break;
	}

	if (writable) {
		if (check_raw_hazard())
			return Stage_Result::RAW;
		if (check_waw_hazard())
			return Stage_Result::WAW;
		fetch_registers(unit);
		if_id.raw_insn = 0;
		return Stage_Result::ID;
	}
	else {
		return Stage_Result::STRUCTURAL;
	}

}

int32_t Pipeline::alu(const IdExAluRegister &id_ex)
{
	switch (id_ex.insn.function)
	{
	case Function::ADD:
		return id_ex.A + id_ex.B;
	case Function::SUB:
		return id_ex.A - id_ex.B;
	case Function::SLL: {
		uint8_t shamt = id_ex.B & 0x1f;
		return (id_ex.A << shamt);
	}
	case Function::SRA: {
		uint8_t shamt = id_ex.B & 0x1f;
		return (id_ex.A >> shamt);
	}
	case Function::SRL: {
		uint8_t shamt = id_ex.B & 0x1f;
		uint32_t A = uint32_t(id_ex.A);
		return uint32_t(A >> shamt);
	}
	case Function::XOR:
		return id_ex.A ^ id_ex.B;
	case Function::OR:
		return id_ex.A | id_ex.B;
	case Function::AND:
		return id_ex.A & id_ex.B;
	case Function::SLT:
		return int32_t(id_ex.A < id_ex.B);
	case Function::SLTU:
		return int32_t(uint32_t(id_ex.A) < uint32_t(id_ex.B));

	case Function::ADDI:
		return id_ex.A + int32_t(id_ex.imm);
	case Function::XORI:
		return id_ex.A ^ int32_t(id_ex.imm);
	case Function::ORI:
		return id_ex.A | int32_t(id_ex.imm);
	case Function::ANDI:
		return id_ex.A & int32_t(id_ex.imm);
	case Function::SLLI:
		return (id_ex.A << id_ex.imm);
	case Function::SRLI: {
		uint32_t A = uint32_t(id_ex.A);
		return uint32_t(A >> id_ex.imm);
	}
	case Function::SRAI:
		return (id_ex.A >> id_ex.imm);
	case Function::SLTI:
		return int32_t(id_ex.A < int32_t(id_ex.imm));
	case Function::SLTIU:
		return int32_t(uint32_t(id_ex.A) < id_ex.imm);

	case Function::BEQ:
		return int32_t(id_ex.A == id_ex.B);
	case Function::BNE:
		return int32_t(id_ex.A != id_ex.B);
	case Function::BLT:
		return int32_t(id_ex.A < id_ex.B);
	case Function::BGE:
		return int32_t(id_ex.A >= id_ex.B);
	case Function::BLTU:
		return int32_t(uint32_t(id_ex.A) < uint32_t(id_ex.B));
	case Function::BGEU:
		return int32_t(uint32_t(id_ex.A) >= uint32_t(id_ex.B));

	}
}

void Pipeline::execute_alu_first_half()
{
	ex_alu.cond = false;
	ex_alu.nop = false;
	ex_alu.syscall_invalidation = false;

	if (id_ex_alu.insn.value == 0) {
		ex_alu.nop = true;
		return;
	}

	switch (id_ex_alu.insn.opcode)
	{
	case Opcode::LUI: {
		ex_alu.aluout = id_ex_alu.imm + 0;
		break;
	}
	case Opcode::AUIPC: {
		ex_alu.aluout = id_ex_alu.imm + id_ex_alu.pc;
		break;
	}
	case Opcode::SYSTEM: {
		ex_alu.aluout = id_ex_alu.pc + WORD_SIZE;
		break;
	}
	case Opcode::JAL:
	case Opcode::JALR: {
		ex_alu.aluout = id_ex_alu.pc + WORD_SIZE;
		ex_alu.cond = true;
		break;
	}
	case Opcode::LOAD:
	case Opcode::LOAD_FP:
	case Opcode::STORE:
	case Opcode::STORE_FP: {
		ex_alu.aluout = int32_t(id_ex_alu.imm)+ id_ex_alu.A;
		break;
	}
	case Opcode::FENCE:
	{
		ex_alu.aluout = 0;
		break;
	}
	case Opcode::AMO: {
		ex_alu.aluout = id_ex_alu.A;
		break;
	}
	case Opcode::BRANCH: {
		int32_t out = alu(id_ex_alu);
		ex_alu.aluout = out;
		if (out) ex_alu.cond = true;
		break;
	}
	case Opcode::OP_IMM: 
	case Opcode::OP: {
		ex_alu.aluout = alu(id_ex_alu);
		break;
	}
	}
}

Stage_Result Pipeline::execute_alu_second_half()
{
	if (ex_alu.nop) {
		return Stage_Result::NOP;
	}

	if (ex_alu.syscall_invalidation) {
		id_ex_alu.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}

	if (ex_alu.cond) {
		iF.cond = true;
		iF.target_addr = id_ex_alu.target_addr;
		id.cond = true;
	}

	if (id_ex_alu.insn.opcode == Opcode::BRANCH) {
		id_ex_alu.insn.value = 0;
		return Stage_Result::EX;
	}
		

	if (ex_mem_alu.insn.value == 0) {
		ex_mem_alu.alu_result = ex_alu.aluout;
		ex_mem_alu.B = id_ex_alu.B;
		ex_mem_alu.Bf = id_ex_alu.Bf;
		ex_mem_alu.insn = id_ex_alu.insn;
		
		id_ex_alu.insn.value = 0;
		return Stage_Result::EX;
	}
	else
		return Stage_Result::STRUCTURAL;
	
}

void Pipeline::execute_muldiv_first_half()
{
	ex_muldiv.nop = false;
	ex_muldiv.syscall_invalidation = false;

	if (id_ex_muldiv.insn.value == 0) {
		ex_muldiv.nop = true;
		return;
	}

	switch (id_ex_muldiv.insn.function)
	{
	case Function::MUL: {
		int64_t out = (id_ex_muldiv.A) * (id_ex_muldiv.B);
		int32_t low = out & 0xFFFFFFFF;
		ex_muldiv.aluout = low;
		return;
	}
	case Function::MULH: {
		int64_t out = int64_t(id_ex_muldiv.A) * int64_t(id_ex_muldiv.B);
		int32_t high = (out & 0xFFFFFFFF00000000) >> 32;
		ex_muldiv.aluout = high;
		return;
	}
	case Function::MULHSU: {
		int64_t out = int64_t(id_ex_muldiv.A) * uint64_t(id_ex_muldiv.B);
		int32_t high = (out & 0xFFFFFFFF00000000) >> 32;
		ex_muldiv.aluout = high;
		return;
	}
	case Function::MULHU: {
		uint64_t out = uint64_t(id_ex_muldiv.A) * uint64_t(id_ex_muldiv.B);
		int32_t high = (out & 0xFFFFFFFF00000000) >> 32;
		ex_muldiv.aluout = high;
		return;
	}
	case Function::DIV: {
		ex_muldiv.aluout = id_ex_muldiv.A / id_ex_muldiv.B;
		return;
	}
	case Function::DIVU: {
		ex_muldiv.aluout = uint32_t(id_ex_muldiv.A) / uint32_t(id_ex_muldiv.B);
		return;
	}
	case Function::REM: {
		ex_muldiv.aluout = id_ex_muldiv.A % id_ex_muldiv.B;
		return;
	}
	case Function::REMU: {
		ex_muldiv.aluout = uint32_t(id_ex_muldiv.A) % uint32_t(id_ex_muldiv.B);
		return;
	}
	}

}

Stage_Result Pipeline::execute_muldiv_second_half()
{
	if (ex_muldiv.nop) {
		return Stage_Result::NOP;
	}

	if (ex_muldiv.syscall_invalidation) {
		id_ex_muldiv.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}


	ex_mem_muldiv.alu_result = ex_muldiv.aluout;
	ex_mem_muldiv.insn = id_ex_muldiv.insn;
	id_ex_muldiv.insn.value = 0;


	return Stage_Result::MULDIV;
}

void Pipeline::execute_fpadd_first_half()
{
	ex_fpadd.nop = false;
	ex_fpadd.syscall_invalidation = false;

	if (id_ex_fpadd.insn.value == 0) {
		ex_fpadd.nop = true;
		return;
	}
	
	if (++(ex_fpadd.step) >= FP_ADD_CYCLE) {
		if (id_ex_fpadd.insn.function == Function::FADD_S)
			ex_fpadd.fpuout = id_ex_fpadd.A + id_ex_fpadd.B;
		else
			ex_fpadd.fpuout = id_ex_fpadd.A - id_ex_fpadd.B;
	}
}

Stage_Result Pipeline::execute_fpadd_second_half()
{
	if (ex_fpadd.nop) {
		return Stage_Result::NOP;
	}

	if (ex_fpadd.syscall_invalidation) {
		ex_fpadd.step = 0;
		id_ex_fpadd.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}

	if (ex_fpadd.step >= FP_ADD_CYCLE) {
		ex_mem_fpadd.fpu_result = ex_fpadd.fpuout;
		ex_mem_fpadd.insn = id_ex_fpadd.insn;
		id_ex_fpadd.insn.value = 0;
		ex_fpadd.step = 0;
	}

	return Stage_Result::FPADD;
}

void Pipeline::execute_fpmul_first_half()
{
	ex_fpmul.nop = false;
	ex_fpmul.syscall_invalidation = false;

	if (id_ex_fpmul.insn.value == 0) {
		ex_fpmul.nop = true;
		return;
	}

	if (++(ex_fpmul.step) >= FP_MUL_CYCLE) {
		ex_fpmul.fpuout = id_ex_fpmul.A * id_ex_fpmul.B;
	}
}

Stage_Result Pipeline::execute_fpmul_second_half()
{
	if (ex_fpmul.nop) {
		return Stage_Result::NOP;
	}

	if (ex_fpmul.syscall_invalidation) {
		ex_fpmul.step = 0;
		id_ex_fpmul.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}

	if (ex_fpmul.step >= FP_MUL_CYCLE) {
		ex_mem_fpmul.fpu_result = ex_fpmul.fpuout;
		ex_mem_fpmul.insn = id_ex_fpmul.insn;
		id_ex_fpmul.insn.value = 0;
		ex_fpmul.step = 0;
	}

	return Stage_Result::FPMUL;
}

void Pipeline::execute_fpdiv_first_half()
{
	ex_fpdiv.nop = false;
	ex_fpdiv.syscall_invalidation = false;
	
	if (id_ex_fpdiv.insn.value == 0) {
		ex_fpdiv.nop = true;
		return;
	}
	
	if (++(ex_fpdiv.step) >= FP_DIV_CYCLE) {
		ex_fpdiv.fpuout = id_ex_fpdiv.A / id_ex_fpdiv.B;
	}
}

Stage_Result Pipeline::execute_fpdiv_second_half()
{
	if (ex_fpdiv.nop) {
		return Stage_Result::NOP;
	}

	if (ex_fpdiv.syscall_invalidation) {
		ex_fpdiv.step = 0;
		id_ex_fpdiv.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}
	
	if (ex_fpdiv.step >= FP_DIV_CYCLE) {
		ex_mem_fpdiv.fpu_result = ex_fpdiv.fpuout;
		ex_mem_fpdiv.insn = id_ex_fpdiv.insn;
		id_ex_fpdiv.insn.value = 0;
		ex_fpdiv.step = 0;
	}

	return Stage_Result::FPDIV;
}

void Pipeline::read_memory()
{
	switch (ex_mem_alu.insn.function)
	{
	case Function::LB:
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, BYTE_SIZE);
		break;
	case Function::LH:
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, HALFWORD_SIZE);
		break;
	case Function::LW:
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		break;
	case Function::LBU:
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, BYTE_SIZE, false);
		break;
	case Function::LHU:
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, HALFWORD_SIZE, false);
		break;
	case Function::FLW:
		mem_alu.mem_result_f = memory->read_float(ex_mem_alu.alu_result);
		break;
	}
}

void Pipeline::write_memory()
{
	switch (ex_mem_alu.insn.function)
	{
	case Function::SB:
		memory->write(ex_mem_alu.alu_result, BYTE_SIZE, (uint32_t*)&ex_mem_alu.B);
		break;
	case Function::SH:
		memory->write(ex_mem_alu.alu_result, HALFWORD_SIZE, (uint32_t*)&ex_mem_alu.B);
		break;
	case Function::SW:
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& ex_mem_alu.B);
		break;
	case Function::FSW:
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& ex_mem_alu.Bf);
		break;
	}
}

void Pipeline::amo()
{
	switch (ex_mem_alu.insn.function)
	{
	case Function::LR_W:
	{
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		break;
	}
	case Function::SC_W:
	{
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)&ex_mem_alu.B);
		mem_alu.mem_result_i = 0;
		break;	
	}
	case Function::AMOSWAP_W: {
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		int32_t result = ex_mem_alu.B;
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)&result);
		break;
	}
	case Function::AMOADD_W:
	{
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		int32_t result = mem_alu.mem_result_i + ex_mem_alu.B;
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& result);
		break;
	}
	case Function::AMOXOR_W:
	{
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		int32_t result = mem_alu.mem_result_i ^ ex_mem_alu.B;
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& result);
		break;
	}
	case Function::AMOAND_W:
	{
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		int32_t result = mem_alu.mem_result_i & ex_mem_alu.B;
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& result);
		break;
	}
	case Function::AMOOR_W:
	{
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		int32_t result = mem_alu.mem_result_i | ex_mem_alu.B;
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& result);
		break;
	}
	case Function::AMOMIN_W:
	{
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		int32_t result = (mem_alu.mem_result_i < ex_mem_alu.B)?mem_alu.mem_result_i:ex_mem_alu.B;
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& result);
		break;
	}
	case Function::AMOMAX_W:
	{
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		int32_t result = (mem_alu.mem_result_i > ex_mem_alu.B) ? mem_alu.mem_result_i : ex_mem_alu.B;
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& result);
		break;
	}
	case Function::AMOMINU_W:
	{
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		uint32_t result = (uint32_t(mem_alu.mem_result_i) < uint32_t(ex_mem_alu.B)) 
			? uint32_t(mem_alu.mem_result_i) : uint32_t(ex_mem_alu.B);
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& result);
		break;
	}
	case Function::AMOMAXU_W:
	{
		mem_alu.mem_result_i = memory->read_int(ex_mem_alu.alu_result, WORD_SIZE);
		uint32_t result = (uint32_t(mem_alu.mem_result_i) > uint32_t(ex_mem_alu.B))
			? uint32_t(mem_alu.mem_result_i) : uint32_t(ex_mem_alu.B);
		memory->write(ex_mem_alu.alu_result, WORD_SIZE, (uint32_t*)& result);
		break;
	}
	}
}

void Pipeline::mem_alu_first_half()
{
	mem_alu.nop = false;
	mem_alu.syscall_invalidation = false;

	if (ex_mem_alu.insn.value == 0) {
		mem_alu.nop = true;
		return;
	}

	if (ex_mem_alu.insn.opcode == Opcode::LOAD
		|| ex_mem_alu.insn.opcode == Opcode::LOAD_FP
		|| ex_mem_alu.insn.opcode == Opcode::STORE
		|| ex_mem_alu.insn.opcode == Opcode::STORE_FP
		|| ex_mem_alu.insn.opcode == Opcode::AMO) {
		if (++mem_alu.step == CACHE_ACCESS_CYCLE) {
			switch (ex_mem_alu.insn.opcode)
			{
			case Opcode::LOAD:
			case Opcode::LOAD_FP:
				read_memory();
				break;
			case Opcode::STORE:
			case Opcode::STORE_FP:
				write_memory();
				break;
			case Opcode::AMO:
				amo();
				break;
			}
		}
	}
}

Stage_Result Pipeline::mem_alu_second_half()
{
	if (mem_alu.nop) {
		return Stage_Result::NOP;
	}

	if (mem_alu.syscall_invalidation) {
		mem_alu.step = 0;
		ex_mem_alu.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}
	if (ex_mem_alu.insn.opcode == Opcode::LOAD
		|| ex_mem_alu.insn.opcode == Opcode::LOAD_FP
		|| ex_mem_alu.insn.opcode == Opcode::STORE
		|| ex_mem_alu.insn.opcode == Opcode::STORE_FP
		|| ex_mem_alu.insn.opcode == Opcode::AMO) {
		if (mem_alu.step == CACHE_ACCESS_CYCLE) {
			if (ex_mem_alu.insn.opcode != Opcode::STORE
				&& ex_mem_alu.insn.opcode != Opcode::STORE_FP) {
				mem_wb_alu.insn = ex_mem_alu.insn;
				mem_wb_alu.mem_result_i = mem_alu.mem_result_i;
				mem_wb_alu.mem_result_f = mem_alu.mem_result_f;
			}
			mem_alu.step = 0;
			ex_mem_alu.insn.value = 0;
		}
	}
	else {
		mem_wb_alu.alu_result = ex_mem_alu.alu_result;
		mem_wb_alu.insn = ex_mem_alu.insn;
		ex_mem_alu.insn.value = 0;
	}
	return Stage_Result::MEM;
}

void Pipeline::mem_muldiv_first_half()
{
	mem_muldiv.nop = false;
	mem_muldiv.syscall_invalidation = false;

	if (ex_mem_muldiv.insn.value == 0) {
		mem_muldiv.nop = true;
		return;
	}
}

Stage_Result Pipeline::mem_muldiv_second_half()
{
	if (mem_muldiv.nop) {
		return Stage_Result::NOP;
	}

	if (mem_muldiv.syscall_invalidation) {
		ex_mem_muldiv.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}

	mem_wb_muldiv.alu_result = ex_mem_muldiv.alu_result;
	mem_wb_muldiv.insn = ex_mem_muldiv.insn;
	ex_mem_muldiv.insn.value = 0;

	return Stage_Result::MEM;
}

void Pipeline::mem_fpadd_first_half()
{
	mem_fpadd.nop = false;
	mem_fpadd.syscall_invalidation = false;

	if (ex_mem_fpadd.insn.value == 0) {
		mem_fpadd.nop = true;
		return;
	}
}

Stage_Result Pipeline::mem_fpadd_second_half()
{
	if (mem_fpadd.nop) {
		return Stage_Result::NOP;
	}

	if (mem_fpadd.syscall_invalidation) {
		ex_mem_fpadd.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}

	mem_wb_fpadd.fpu_result = ex_mem_fpadd.fpu_result;
	mem_wb_fpadd.insn = ex_mem_fpadd.insn;
	ex_mem_fpadd.insn.value = 0;

	return Stage_Result::MEM;
}

void Pipeline::mem_fpmul_first_half()
{
	mem_fpmul.nop = false;
	mem_fpmul.syscall_invalidation = false;

	if (ex_mem_fpmul.insn.value == 0) {
		mem_fpmul.nop = true;
		return;
	}
}

Stage_Result Pipeline::mem_fpmul_second_half()
{
	if (mem_fpmul.nop) {
		return Stage_Result::NOP;
	}

	if (mem_fpmul.syscall_invalidation) {
		ex_mem_fpmul.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}

	mem_wb_fpmul.fpu_result = ex_mem_fpmul.fpu_result;
	mem_wb_fpmul.insn = ex_mem_fpmul.insn;
	ex_mem_fpmul.insn.value = 0;

	return Stage_Result::MEM;
}

void Pipeline::mem_fpdiv_first_half()
{
	mem_fpdiv.nop = false;
	mem_fpdiv.syscall_invalidation = false;

	if (ex_mem_fpdiv.insn.value == 0) {
		mem_fpdiv.nop = true;
		return;
	}
}

Stage_Result Pipeline::mem_fpdiv_second_half()
{
	if (mem_fpdiv.nop) {
		return Stage_Result::NOP;
	}

	if (mem_fpdiv.syscall_invalidation) {
		ex_mem_fpdiv.insn.value = 0;
		return Stage_Result::SYSCALL_STALL;
	}

	mem_wb_fpdiv.fpu_result = ex_mem_fpdiv.fpu_result;
	mem_wb_fpdiv.insn = ex_mem_fpdiv.insn;
	ex_mem_fpdiv.insn.value = 0;

	return Stage_Result::MEM;
}

int Pipeline::wb_alu_first_half(unsigned long long clock)
{
	wb_alu.nop = false;

	if (mem_wb_alu.insn.value == 0) {
		wb_alu.nop = true;
		return 0;
	}

	uint32_t rd = mem_wb_alu.insn.fields.rd;
	if (rd == 0 && mem_wb_alu.insn.opcode != Opcode::SYSTEM) return 0;


	switch (mem_wb_alu.insn.opcode)
	{
	case Opcode::LOAD:
	case Opcode::AMO: {
		register_file.gpr[rd] = mem_wb_alu.mem_result_i;
		return 0;
	}
	case Opcode::LOAD_FP: {
		register_file.fpr[rd] = mem_wb_alu.mem_result_f;
		return 0;
	}
	case Opcode::SYSTEM:{
		if (mem_wb_alu.insn.function == Function::ECALL) {
			iF.syscall_invalidation = true;
			id.syscall_invalidation = true;
			ex_alu.syscall_invalidation = true;
			ex_muldiv.syscall_invalidation = true;
			ex_fpadd.syscall_invalidation = true;
			ex_fpmul.syscall_invalidation = true;
			ex_fpdiv.syscall_invalidation = true;

			mem_alu.syscall_invalidation = true;
			mem_muldiv.syscall_invalidation = true;
			mem_fpadd.syscall_invalidation = true;
			mem_fpmul.syscall_invalidation = true;
			mem_fpdiv.syscall_invalidation = true;

			handle_syscall(register_file, *memory, clock);
			register_file.pc = mem_wb_alu.alu_result;
		}
		return 0;
	}
	case Opcode::FENCE:
		return 0;
	default: {
		register_file.gpr[rd] = mem_wb_alu.alu_result;
		return 0;
	}
	}
}

Stage_Result Pipeline::wb_alu_second_half()
{
	if (wb_alu.nop) {
		return Stage_Result::NOP;
	}
	mem_wb_alu.insn.value = 0;

	return Stage_Result::WB;
}

void Pipeline::wb_muldiv_first_half()
{
	wb_muldiv.nop = false;

	if (mem_wb_muldiv.insn.value == 0) {
		wb_muldiv.nop = true;
		return;
	}

	uint32_t rd = mem_wb_muldiv.insn.fields.rd;
	if (rd == 0) return;
	int32_t value = mem_wb_muldiv.alu_result;

	register_file.gpr[rd] = value;
}

Stage_Result Pipeline::wb_muldiv_second_half()
{
	if (wb_muldiv.nop) {
		return Stage_Result::NOP;
	}
	mem_wb_muldiv.insn.value = 0;

	return Stage_Result::WB;
}

void Pipeline::wb_fpadd_first_half()
{
	wb_fpadd.nop = false;

	if (mem_wb_fpadd.insn.value == 0) {
		wb_fpadd.nop = true;
		return;
	}

	uint32_t rd = mem_wb_fpadd.insn.fields.rd;
	float value = mem_wb_fpadd.fpu_result;

	register_file.fpr[rd] = value;
}

Stage_Result Pipeline::wb_fpadd_second_half()
{
	if (wb_fpadd.nop) {
		return Stage_Result::NOP;
	}
	mem_wb_fpadd.insn.value = 0;

	return Stage_Result::WB;
}

void Pipeline::wb_fpmul_first_half()
{
	wb_fpmul.nop = false;

	if (mem_wb_fpmul.insn.value == 0) {
		wb_fpmul.nop = true;
		return;
	}

	uint32_t rd = mem_wb_fpmul.insn.fields.rd;
	float value = mem_wb_fpmul.fpu_result;

	register_file.fpr[rd] = value;
}

Stage_Result Pipeline::wb_fpmul_second_half()
{
	if (wb_fpmul.nop) {
		return Stage_Result::NOP;
	}
	mem_wb_fpmul.insn.value = 0;

	return Stage_Result::WB;
}

void Pipeline::wb_fpdiv_first_half()
{
	wb_fpdiv.nop = false;

	if (mem_wb_fpdiv.insn.value == 0) {
		wb_fpdiv.nop = true;
		return;
	}

	uint32_t rd = mem_wb_fpdiv.insn.fields.rd;
	float value = mem_wb_fpdiv.fpu_result;

	register_file.fpr[rd] = value;
}

Stage_Result Pipeline::wb_fpdiv_second_half()
{
	if (wb_fpdiv.nop) {
		return Stage_Result::NOP;
	}
	mem_wb_fpdiv.insn.value = 0;

	return Stage_Result::WB;
}


void Pipeline::run()
{
	unsigned long long clock = 1;

	while (true)
	{
		int shut_down = 0;
		uint32_t fetch = 0;
		fetch_first_half();
		fetch = register_file.pc;
		id_first_half();
		execute_alu_first_half();
		execute_muldiv_first_half();
		execute_fpadd_first_half();
		execute_fpmul_first_half();
		execute_fpdiv_first_half();
		mem_alu_first_half();
		mem_muldiv_first_half();
		mem_fpadd_first_half();
		mem_fpmul_first_half();
		mem_fpdiv_first_half();
		shut_down = wb_alu_first_half(clock);
		wb_muldiv_first_half();
		wb_fpadd_first_half();
		wb_fpmul_first_half();
		wb_fpdiv_first_half();

		Stage_Result results[17];
		Instruction out[17]{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
		//Function func_results[17];
		//std::fill_n(func_results, 17, Function::NOP);

		results[16] = wb_alu_second_half();
		if (results[16] != Stage_Result::NOP) {
			out[16] = mem_wb_alu.insn;
		}
		results[15] = wb_muldiv_second_half();
		if (results[15] != Stage_Result::NOP) {
			out[15] = mem_wb_muldiv.insn;
		}
		results[14] = wb_fpadd_second_half();
		if (results[14] != Stage_Result::NOP) {
			out[14] = mem_wb_fpadd.insn;
		}
		results[13] = wb_fpmul_second_half();
		if (results[13] != Stage_Result::NOP) {
			out[13] = mem_wb_fpmul.insn;
		}
		results[12] = wb_fpdiv_second_half();
		if (results[12] != Stage_Result::NOP) {
			out[12] = mem_wb_fpdiv.insn;
		}

		results[11] = mem_alu_second_half();
		if (results[11] != Stage_Result::NOP) {
			out[11] = ex_mem_alu.insn;
		}
		results[10] = mem_muldiv_second_half();
		if (results[10] != Stage_Result::NOP) {
			out[10] = ex_mem_muldiv.insn;
		}
		results[9] = mem_fpadd_second_half();
		if (results[9] != Stage_Result::NOP) {
			out[9] = ex_mem_fpadd.insn;
		}
		results[8] = mem_fpmul_second_half();
		if (results[8] != Stage_Result::NOP) {
			out[8] = ex_mem_fpmul.insn;
		}
		results[7] = mem_fpdiv_second_half();
		if (results[7] != Stage_Result::NOP) {
			out[7] = ex_mem_fpdiv.insn;
		}

		results[6] = execute_alu_second_half();
		if (results[6] != Stage_Result::NOP) {
			out[6] = id_ex_alu.insn;
		}
		results[5] = execute_muldiv_second_half();
		if (results[5] != Stage_Result::NOP) {
			out[5] = id_ex_muldiv.insn;
		}
		results[4] = execute_fpadd_second_half();
		if (results[4] != Stage_Result::NOP) {
			out[4] = id_ex_fpadd.insn;
		}
		results[3] = execute_fpmul_second_half();
		if (results[3] != Stage_Result::NOP) {
			out[3] = id_ex_fpmul.insn;
		}
		results[2] = execute_fpdiv_second_half();
		if (results[2] != Stage_Result::NOP) {
			out[2] = id_ex_fpdiv.insn;
		}

		results[1] = id_second_half();
		if (results[1] != Stage_Result::NOP) {
			out[1] = id.insn;
		}

		results[0] = fetch_second_half();

		++clock;
        /*
        const char* stage_str[16] = {
			"IF", "SYSCALL", "BRANCH",
			"STRUCT", "SYNC",
			"ID", "RAW", "WAW", "NOP",
			"FPDIV", "FPMUL", "FPADD",
			"MD", "EX",
			"MEM", "WB"
		};
		const char* func_str[67] = {
			"LUI", "AUIPC", "JAL",        
			"JALR",       
			"BEQ",        
			"BNE",        
			"BLT",        
			"BGE",        
			"BLTU",       
			"BGEU",       
			"LB",         
			"LH",         
			"LW",         
			"LBU",        
			"LHU",        
			"SB",         
			"SH",         
			"SW",         
			"ADDI",       
			"SLTI",       
			"SLTIU",      
			"XORI",       
			"ORI",        
			"ANDI",       
			"SLLI",       
			"SRLI",       
			"SRAI",       
			"ADD",        
			"SUB",        
			"SLL",        
			"SLT",        
			"SLTU",       
			"XOR",        
			"SRL",        
			"SRA",        
			"OR",         
			"AND",        
			"MUL",        
			"MULH",       
			"MULHSU",     
			"MULHU",      
			"DIV",        
			"DIVU",       
			"REM",        
			"REMU",       
			"ECALL",      
			"EBREAK",		
			"LR_W",
			"SC_W",
			"AMOSWAP_W",
			"AMOADD_W",
			"AMOXOR_W",
			"AMOAND_W",
			"AMOOR_W",
			"AMOMIN_W",
			"AMOMAX_W",
			"AMOMINU_W",
			"AMOMAXU_W",
			"FLW",        
			"FSW",        
			"FADD_S",
			"FSUB_S",
			"FMUL_S",
			"FDIV_S",
			"FENCE",
			"FENCE_I",
			"_"
		};

		std::clog << "clock " << std::dec<< clock << std::endl;
		std::clog << stage_str[static_cast<int>(results[0])] << ":" << std::hex << fetch << std::endl;
		std::clog << stage_str[static_cast<int>(results[1])];
		if (results[1] != Stage_Result::NOP) {
			std::clog << ":" << func_str[static_cast<int>(out[1].function)]
				<< "[" << "pc:" << std::hex <<  out[1].fields.pc
				<< " rs1:" << std::hex << out[1].fields.rs1
				<< " rs2:" << std::hex << out[1].fields.rs2
				<< " rd:" << std::hex << out[1].fields.rd
				<< " imm:" << std::hex << out[1].fields.imm
				<< "(" << std::dec << int(out[1].fields.imm) << ")]";
		}
		std::clog << std::endl;

		for (int i = 2; i < 17; ++i) {
			std::clog << stage_str[static_cast<int>(results[i])];
			if (results[i] != Stage_Result::NOP) {
				std::clog << ":" << func_str[static_cast<int>(out[i].function)]
					<< "[" << "pc:" << std::hex << out[i].fields.pc
					<< " rs1:" << std::hex << out[i].fields.rs1
					<< " rs2:" << std::hex << out[i].fields.rs2
					<< " rd:" << std::hex << out[i].fields.rd
					<< " imm:" << std::hex << out[i].fields.imm << "]";
			}
			std::clog << "|";
			if ((i - 1) % 5 == 0)
				std::clog << std::endl;
		}

        for(int i = 0; i < 32; ++i)
            std::clog << " [" << i <<"]:" << std::hex << register_file.gpr[i];
        std::clog << std::endl;
	    */	
        if (shut_down)
			return;
	}
}

