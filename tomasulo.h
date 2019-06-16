#pragma once
#include "instruction.h"
#include "consts.h"
#include "memory.h"
#include "registers.h"
#include <deque>
#include <list>

struct RS_ENTRY {
	Function function;
	Opcode opcode;
	bool in_use{ false };

	int32_t Vj, Vk;
	uint32_t Qj, Qk;
	uint32_t dest;		// ROB number
	uint32_t A;

	uint32_t cycle{ 0 };
	int32_t result;

};

struct ROB_ENTRY
{
	Instruction insn;
	uint32_t rd, addr;
	bool complete{ false };
	int32_t value;

	int32_t mem_value;	// for AMO
	// for STORE
	bool ready_value{ false }, ready_addr{ false };
	uint32_t cycle{ 0 };
	uint32_t src{ 0 };

	ROB_ENTRY(Instruction& insn) : insn(insn) {};
};

struct CDB_ENTRY
{
	uint32_t nROB;
	int32_t value;

	CDB_ENTRY(uint32_t rob, uint32_t v)
		: nROB(rob), value(v){}
};

struct REGISTER_STATE {
	bool busy{ false };
	uint32_t nROB;
};

class Tomasulo {
	Memory* memory{ nullptr };
	RegisterFile register_file;
	REGISTER_STATE register_stat[32];

	std::deque<Instruction> instrunction_queue;
	std::list<ROB_ENTRY> ROB_queue;
	std::list<RS_ENTRY> ALU_RS;
	std::list<RS_ENTRY> MULDIV_RS;
	std::list<RS_ENTRY> ADDR_RS;
	std::list<RS_ENTRY> LOAD_BUFFER;

private:
	bool get_operand(uint32_t rg, int32_t& value, uint32_t& nROB);

	Stage_Result fetch_n_decode();

	void fill_RSentry(const Instruction& insn, RS_ENTRY& rs, uint32_t nROB);
	Stage_Result issue();

	Stage_Result execute_alu();
	Stage_Result execute_muldiv();
	Stage_Result execute_addr_unit();

	bool find_mem_value_in_ROB(uint32_t start_el, uint32_t addr, bool& valid, int32_t& value);
	int32_t amo(Function func, int32_t load_value, int32_t src);
	int32_t read_memory(Function func, int32_t addr);
	void write_memory(Function func, int32_t addr, int32_t value);
	Stage_Result execute_memory_unit();

	void get_ALU_RS_completion(std::list<CDB_ENTRY>&cdb);
	void get_MULDIV_RS_completion(std::list<CDB_ENTRY>&cdb);
	void get_LOAD_BUFFER_completion(std::list<CDB_ENTRY>&cdb);
	void broadcast(CDB_ENTRY cdb);
	Stage_Result write_result();

	void ROB_clear();
	Stage_Result commit(unsigned long long clock);
public:
    Tomasulo(Memory* mem, uint32_t entry_point, uint32_t sp) : memory(mem) {
		register_file.pc = entry_point;
		register_file.gpr[2] = sp;
	}

	void run();
};
