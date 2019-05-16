#pragma once
#include "instruction.h"
#include "consts.h"
#include "memory.h"
#include "registers.h"

// IF/ID
struct IfIdRegister {
	/// Program Counter
	uint32_t pc{ 0 };
	/// Raw instruction
	uint32_t raw_insn{ 0 };
	// raw_insn == 0 -> NOP
};

// ID/EX

struct IdExAluRegister {
	uint32_t pc{ 0 };
	Instruction insn{ 0 };

	int32_t A{ 0 };
	int32_t B{ 0 };
	uint32_t imm{ 0 };

	uint32_t target_addr{ 0 };
	
	float Bf{ 0.f };
};


struct IdExMuldivRegister {
	Instruction insn{ 0 };
	int32_t A{ 0 };
	int32_t B{ 0 };
};


struct IdExFpuRegister {
	Instruction insn{ 0 };
	float A{ 0.f };
	float B{ 0.f };

};

// EX/MEM

struct ExMemAluRegister {
	Instruction insn{ 0 };
	int32_t alu_result{ 0 };
	int32_t B{ 0 };

	float Bf{ 0.f };

};

struct ExMemMuldivRegister {
	Instruction insn{ 0 };
	int32_t alu_result{ 0 };
};

struct ExMemFpuRegister {
	Instruction insn{ 0 };
	float fpu_result{ 0 };
};

// MEM/WB
struct MemWbAluRegister {
	Instruction insn{ 0 };
	int32_t alu_result{ 0 };

	int32_t mem_result_i{ 0 };
	float mem_result_f{ 0.f };
};

struct MemWbMuldivRegister {
	Instruction insn{ 0 };
	int32_t alu_result{ 0 };
};

struct MemWbFpuRegister {
	Instruction insn{ 0 };
	float fpu_result{ 0 };
};

struct IF {
	uint32_t raw_insn{ 0 };
	uint32_t target_addr{0 };

	bool cond{ false };
	bool syscall_invalidation{ false };
};

struct ID {
	Instruction insn{ 0 };

	bool syscall_invalidation{ false };
	bool cond{ false };
};

struct EX_INT
{
	int32_t aluout{ 0 };
	bool cond{ false };

	bool nop{ false };
	bool syscall_invalidation{ false };
};

struct EX_FP
{
	float fpuout{ 0.f };
	uint8_t step{ 0 };
	
	bool nop{ false };
	bool syscall_invalidation{ false };
};

struct MEM_ALU
{
	bool nop{ false };
	bool syscall_invalidation{ false };

	int32_t mem_result_i{ 0 };
	float mem_result_f{ 0.f };
	
	uint8_t step{ 0 };
};

struct MEM
{
	bool nop{ false };
	bool syscall_invalidation{ false };
};

struct WB
{
	bool nop{ false };
};


// Pipeline

class Pipeline {
	IfIdRegister if_id;

	IdExAluRegister id_ex_alu;
	IdExMuldivRegister id_ex_muldiv;
	IdExFpuRegister id_ex_fpadd;
	IdExFpuRegister id_ex_fpmul;
	IdExFpuRegister id_ex_fpdiv;

	ExMemAluRegister ex_mem_alu;
	ExMemMuldivRegister ex_mem_muldiv;
	ExMemFpuRegister ex_mem_fpadd;
	ExMemFpuRegister ex_mem_fpmul;
	ExMemFpuRegister ex_mem_fpdiv;

	MemWbAluRegister mem_wb_alu;
	MemWbMuldivRegister mem_wb_muldiv;
	MemWbFpuRegister mem_wb_fpadd;
	MemWbFpuRegister mem_wb_fpmul;
	MemWbFpuRegister mem_wb_fpdiv;

	// stage variable
	IF iF;
	ID id;
	EX_INT ex_alu;
	EX_INT ex_muldiv;
	EX_FP ex_fpadd;
	EX_FP ex_fpmul;
	EX_FP ex_fpdiv;

	MEM_ALU mem_alu;
	MEM mem_muldiv;
	MEM mem_fpadd;
	MEM mem_fpmul;
	MEM mem_fpdiv;
	
	WB wb_alu;
	WB wb_muldiv;
	WB wb_fpadd;
	WB wb_fpmul;
	WB wb_fpdiv;

	Memory* memory{ nullptr };
	RegisterFile register_file;

private:	
	void fetch_first_half();
	Stage_Result fetch_second_half();

	void id_first_half();
	bool is_syscall_sync_insn();
	bool check_gpr_dependency(uint32_t rs);
	bool check_fpr_dependency(uint32_t rs);
	bool check_raw_hazard();
	bool check_fpr_waw_hazard(uint32_t rd);
	bool check_gpr_waw_hazard(uint32_t rd);
	bool check_waw_hazard();
	int32_t fetch_gpr(uint32_t rg);
	float fetch_fpr(uint32_t rg);
	void fetch_registers(UNIT unit);
	Stage_Result id_second_half();

	int32_t alu(const IdExAluRegister &id_ex);
	void execute_alu_first_half();
	Stage_Result execute_alu_second_half();
	void execute_muldiv_first_half();
	Stage_Result execute_muldiv_second_half();
	void execute_fpadd_first_half();
	Stage_Result execute_fpadd_second_half();
	void execute_fpmul_first_half();
	Stage_Result execute_fpmul_second_half();
	void execute_fpdiv_first_half();
	Stage_Result execute_fpdiv_second_half();

	void read_memory();
	void write_memory();
	void amo();
	void mem_alu_first_half();
	Stage_Result mem_alu_second_half();
	void mem_muldiv_first_half();
	Stage_Result mem_muldiv_second_half();
	void mem_fpadd_first_half();
	Stage_Result mem_fpadd_second_half();
	void mem_fpmul_first_half();
	Stage_Result mem_fpmul_second_half();
	void mem_fpdiv_first_half();
	Stage_Result mem_fpdiv_second_half();

	int wb_alu_first_half();
	Stage_Result wb_alu_second_half();
	void wb_muldiv_first_half();
	Stage_Result wb_muldiv_second_half();
	void wb_fpadd_first_half();
	Stage_Result wb_fpadd_second_half();
	void wb_fpmul_first_half();
	Stage_Result wb_fpmul_second_half();
	void wb_fpdiv_first_half();
	Stage_Result wb_fpdiv_second_half();

	

public:
	Pipeline(Memory* mem, uint32_t entry_point, uint32_t sp) : memory(mem) {
		register_file.pc = entry_point;
		register_file.gpr[2] = sp;
	}

	
	void run();

};

