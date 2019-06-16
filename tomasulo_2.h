#pragma once
#include "instruction.h"
#include "consts.h"
#include "memory.h"
#include "registers.h"
#include "tomasulo.h"
#include <deque>
#include <list>

struct TWO_BIT_ENTRY{
    bool taken{ true };
    bool miss{ false };
};

class Tomasulo_Two {
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
    Tomasulo_Two(Memory* mem, uint32_t entry_point, uint32_t sp) : memory(mem) {
		register_file.pc = entry_point;
		register_file.gpr[2] = sp;
	}

	void run();
};
