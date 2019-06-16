#include "tomasulo.h"
#include "syscall.h"
#include <iostream>


bool Tomasulo::get_operand(uint32_t rg, int32_t & value, uint32_t & nROB)
{
	if (rg == 0) {
		value = 0;
		nROB = 0;
		return true;
	}
	if (register_stat[rg].busy == true) {
		ROB_ENTRY* src = (ROB_ENTRY*)(register_stat[rg].nROB);
		if (src->complete) {
			value = src->value;
			nROB = 0;
			return true;
		}
		else {
			value = 0;
			nROB = uint32_t(src);
			return false;
		}
	}

	value = register_file.gpr[rg];
	nROB = 0;
	return true;
}

Stage_Result Tomasulo::fetch_n_decode()
{
	uint32_t raw_insn = memory->read_int(
		uint32_t(register_file.pc), WORD_SIZE);

	Instruction insn{ raw_insn };
	insn.fields.pc = register_file.pc;
	insn.decode();

    if(insn.opcode == Opcode::STORE_FP || insn.opcode == Opcode::LOAD_FP || insn.opcode == Opcode::OP_FP){
        Instruction nop{ 0x13 };
        nop.fields.pc = register_file.pc;
        nop.decode();
        insn = nop;
    }

	if (insn.opcode == Opcode::BRANCH
		|| insn.opcode == Opcode::JAL) {
		register_file.pc += int32_t(insn.fields.imm);
		insn.taken = true;
	}
	else if (insn.opcode == Opcode::JALR) {
		int32_t value{ 0 };
		uint32_t nROB{ 0 };
		bool availabe = get_operand(insn.fields.rs1, value, nROB );
		if (availabe == false)
			return Stage_Result::RAW;

		register_file.pc = int32_t(insn.fields.imm) + value;
		register_file.pc = register_file.pc & 0xfffffffe; // LSB -> 0
		insn.taken = true;
	}
	else {
		register_file.pc += WORD_SIZE;
	}

	instrunction_queue.emplace_back(insn);

	return Stage_Result::IF;
}

void Tomasulo::fill_RSentry(const Instruction & insn, RS_ENTRY & rs, uint32_t nROB)
{
	rs.function = insn.function;
	rs.opcode = insn.opcode;
	rs.in_use = true;
	rs.dest = nROB;

	switch (insn.opcode)
	{
	case Opcode::JAL:
	case Opcode::JALR:
	{
		rs.Vj = insn.fields.pc;
		rs.Vk = WORD_SIZE;
		rs.Qj = 0; rs.Qk = 0;
		return;
	}
	case Opcode::SYSTEM:
	{
		rs.Vj = insn.fields.pc;
		rs.Vk = WORD_SIZE;
		rs.Qj = 0; rs.Qk = 0;
		return;
	}
	case Opcode::LUI:
	{
		rs.Vj = insn.fields.imm;
		rs.Vk = 0;
		rs.Qj = 0; rs.Qk = 0;
		return;
	}
	case Opcode::AUIPC:
	{
		rs.Vj = insn.fields.imm;
		rs.Vk = insn.fields.pc;
		rs.Qj = 0; rs.Qk = 0;
		return;
	}
	case Opcode::FENCE:
	{
		rs.Vj = 0; rs.Vk = 0;
		rs.Qj = 0; rs.Qk = 0;
		return;
	}
	case Opcode::LOAD:
	case Opcode::STORE:
	{
		uint32_t nROB;
		int32_t value;
		bool availabe = get_operand(insn.fields.rs1, value, nROB);
		if (availabe) {
			rs.Vj = value;
			rs.Qj = 0;
		}
		else {
			rs.Vj = 0;
			rs.Qj = nROB;
		}

		rs.Vk = 0; rs.Qk = 0;
		rs.A = insn.fields.imm;
		return;
	}
	case Opcode::AMO:
	{
		uint32_t nROB;
		int32_t value;
		bool availabe = get_operand(insn.fields.rs1, value, nROB);
		if (availabe) {
			rs.Vj = value;
			rs.Qj = 0;
		}
		else {
			rs.Vj = 0;
			rs.Qj = nROB;
		}
		if (insn.function == Function::SC_W) {
			rs.Vk = 0; rs.Qk = 0;
			return;
		}
		availabe = get_operand(insn.fields.rs2, value, nROB);
		if (availabe) {
			rs.Vk = value; rs.Qk = 0;
		}
		else {
			rs.Vk = 0; rs.Qk = nROB;
		}
		return;
	}
	case Opcode::OP_IMM:
	{
		uint32_t nROB;
		int32_t value;
		bool availabe = get_operand(insn.fields.rs1, value, nROB);
		if (availabe) {
			rs.Vj = value;
			rs.Qj = 0;
		}
		else {
			rs.Vj = 0;
			rs.Qj = nROB;
		}

		rs.Vk = insn.fields.imm; rs.Qk = 0;
		return;
	}

	case Opcode::BRANCH:
	case Opcode::OP:
	{
		uint32_t nROB;
		int32_t value;
		bool availabe = get_operand(insn.fields.rs1, value, nROB);
		if (availabe) {
			rs.Vj = value;
			rs.Qj = 0;
		}
		else {
			rs.Vj = 0;
			rs.Qj = nROB;
		}
		availabe = get_operand(insn.fields.rs2, value, nROB);
		if (availabe) {
			rs.Vk = value; rs.Qk = 0;
		}
		else {
			rs.Vk = 0; rs.Qk = nROB;
		}
		return;
	}
	default:
		std::clog << "Invalid operation" << std::endl;
		exit(1);
	}
}

Stage_Result Tomasulo::issue()
{
	if (instrunction_queue.empty())
		return Stage_Result::NOP;
	
	Instruction& insn = instrunction_queue.front();

	// create a ROB entry
	ROB_queue.emplace_back(insn);
	ROB_queue.back().rd = insn.fields.rd;
	if (insn.function == Function::ECALL)
		ROB_queue.back().rd = 10;
    if(insn.opcode == Opcode::STORE){
        ROB_queue.back().complete = true;
    }
	if (insn.opcode == Opcode::STORE
		||(insn.function == Function::SC_W)) {
		uint32_t nROB;
		int32_t value;
		bool availabe = get_operand(insn.fields.rs2, value, nROB);
		if (availabe) {
			ROB_queue.back().mem_value = value;
			ROB_queue.back().ready_value = true;
			ROB_queue.back().src = 0;
		}
		else {
			ROB_queue.back().src = nROB;
			ROB_queue.back().mem_value = 0;
			ROB_queue.back().ready_value = false;
		}
	}


	// cread a RS entry
	RS_ENTRY rs;
	fill_RSentry(insn, rs, uint32_t(&ROB_queue.back()));

	if (insn.opcode == Opcode::STORE
		|| insn.opcode == Opcode::LOAD
		|| insn.opcode == Opcode::AMO)
	{
		ADDR_RS.emplace_back(rs);
	}
	else
	{
		switch (insn.function)
		{
		case Function::MUL:
		case Function::MULH:
		case Function::MULHSU:
		case Function::MULHU:
		case Function::DIV:
		case Function::DIVU:
		case Function::REM:
		case Function::REMU: {
			MULDIV_RS.emplace_back(rs);
			break;
		}
		default: {
			ALU_RS.emplace_back(rs);
			break;
		}
		}
	}

	instrunction_queue.pop_front();
	if (ROB_queue.back().rd != 0) {
		register_stat[ROB_queue.back().rd].busy = true;
		register_stat[ROB_queue.back().rd].nROB = uint32_t(&ROB_queue.back());
	}

	return Stage_Result::ISSUE;
}

Stage_Result Tomasulo::execute_alu()
{
	if (ALU_RS.empty())
		return Stage_Result::NOP;
	
	std::list<RS_ENTRY>::iterator i = ALU_RS.begin();
	while (i != ALU_RS.end()) {
		if (i->Qj == 0 && i->Qk == 0) {
			switch (i->function)
			{
			case Function::ADD:
				++(i->cycle);
				i->result = i->Vj + i->Vk;
				break;
			case Function::SUB:
				++(i->cycle);
				i->result = i->Vj - i->Vk;
				break;
			case Function::SLL: {
				++(i->cycle);
				uint8_t shamt = i->Vk & 0x1f;
				i->result = (i->Vj << shamt);
				break;
			}
			case Function::SRA: {
				++(i->cycle);
				uint8_t shamt = i->Vk & 0x1f;
				i->result = (i->Vj >> shamt);
				break;
			}
			case Function::SRL: {
				++(i->cycle);
				uint8_t shamt = i->Vk & 0x1f;
				uint32_t A = uint32_t(i->Vj);
				i->result = uint32_t(A >> shamt);
				break;
			}
			case Function::XOR:
				++(i->cycle);
				i->result = i->Vj ^ i->Vk;
				break;
			case Function::OR:
				++(i->cycle);
				i->result = i->Vj | i->Vk;
				break;
			case Function::AND:
				++(i->cycle);
				i->result = i->Vj & i->Vk;
				break;
			case Function::SLT:
				++(i->cycle);
				i->result = int32_t(i->Vj < i->Vk);
				break;
			case Function::SLTU:
				++(i->cycle);
				i->result = int32_t(uint32_t(i->Vj) < uint32_t(i->Vk));
				break;

			case Function::ADDI:
				++(i->cycle);
				i->result = i->Vj + int32_t(i->Vk);
				break;
			case Function::XORI:
				++(i->cycle);
				i->result = i->Vj ^ int32_t(i->Vk);
				break;
			case Function::ORI:
				++(i->cycle);
				i->result = i->Vj | int32_t(i->Vk);
				break;
			case Function::ANDI:
				++(i->cycle);
				i->result = i->Vj & int32_t(i->Vk);
				break;
			case Function::SLLI:
				++(i->cycle);
				i->result = (i->Vj << i->Vk);
				break;
			case Function::SRLI: {
				++(i->cycle);
				uint32_t A = uint32_t(i->Vj);
				i->result = uint32_t(A >> i->Vk);
				break;
			}
			case Function::SRAI:
				++(i->cycle);
				i->result = (i->Vj >> i->Vk);
				break;
			case Function::SLTI:
				++(i->cycle);
				i->result = int32_t(i->Vj < int32_t(i->Vk));
				break;
			case Function::SLTIU:
				++(i->cycle);
				i->result = int32_t(uint32_t(i->Vj) < i->Vk);
				break;

			case Function::BEQ:
				++(i->cycle);
				i->result = int32_t(i->Vj == i->Vk);
				break;
			case Function::BNE:
				++(i->cycle);
				i->result = int32_t(i->Vj != i->Vk);
				break;
			case Function::BLT:
				++(i->cycle);
				i->result = int32_t(i->Vj < i->Vk);
				break;
			case Function::BGE:
				++(i->cycle);
				i->result = int32_t(i->Vj >= i->Vk);
				break;
			case Function::BLTU:
				++(i->cycle);
				i->result = int32_t(uint32_t(i->Vj) < uint32_t(i->Vk));
				break;
			case Function::BGEU:
				++(i->cycle);
				i->result = int32_t(uint32_t(i->Vj) >= uint32_t(i->Vk));
				break;

			default:
				++(i->cycle);
				i->result = i->Vj + i->Vk;
				break;
			}
		}
		++i;
	}

	return Stage_Result::EX;
}

Stage_Result Tomasulo::execute_muldiv()
{
	if (MULDIV_RS.empty())
		return Stage_Result::NOP;

	std::list<RS_ENTRY>::iterator i = MULDIV_RS.begin();
	while (i != MULDIV_RS.end())
	{
		if (i->Qj == 0 && i->Qk == 0) {
			switch (i->function)
			{
			case Function::MUL: {
				if (++(i->cycle) >= MUL_CYCLE) {
					int64_t out = (i->Vj) * (i->Vk);
					int32_t low = out & 0xFFFFFFFF;
					i->result = low;
				}
				break;
			}
			case Function::MULH: {
				if (++(i->cycle) >= MUL_CYCLE) {
					int64_t out = int64_t(i->Vj) * int64_t(i->Vk);
					int32_t high = (out & 0xFFFFFFFF00000000) >> 32;
					i->result = high;
				}
				break;
			}
			case Function::MULHSU: {
				if (++(i->cycle) >= MUL_CYCLE) {
					int64_t out = int64_t(i->Vj) * uint64_t(i->Vk);
					int32_t high = (out & 0xFFFFFFFF00000000) >> 32;
					i->result = high;
				}
				break;
			}
			case Function::MULHU: {
				if (++(i->cycle) >= MUL_CYCLE) {
					uint64_t out = uint64_t(i->Vj) * uint64_t(i->Vk);
					int32_t high = (out & 0xFFFFFFFF00000000) >> 32;
					i->result = high;
				}
				break;
			}
			case Function::DIV: {
				if (++(i->cycle) >= DIV_CYCLE) {
					i->result = i->Vj / i->Vk;
				}
				break;
			}
			case Function::DIVU: {
				if (++(i->cycle) >= DIV_CYCLE) {
					i->result = uint32_t(i->Vj) / uint32_t(i->Vk);
				}
				break;
			}
			case Function::REM: {
				if (++(i->cycle) >= DIV_CYCLE) {
					i->result = i->Vj % i->Vk;

				}
				break;
			}
			case Function::REMU: {
				if (++(i->cycle) >= DIV_CYCLE) {
					i->result = uint32_t(i->Vj) % uint32_t(i->Vk);
				}
				break;
			}
			}
			
		}
		++i;
		
	}

	
	return Stage_Result::MULDIV;
}

Stage_Result Tomasulo::execute_addr_unit()
{
	if (ADDR_RS.empty())
		return Stage_Result::NOP;

	std::list<RS_ENTRY>::iterator i = ADDR_RS.begin();
	while (i != ADDR_RS.end()) {
		if (i->Qj == 0 && i->Qk == 0) {
			switch (i->opcode)
			{
			case Opcode::LOAD: {
				RS_ENTRY rs;
				rs.function = i->function;
				rs.opcode = i->opcode;
				rs.in_use = true;
				rs.Vj = 0; rs.Vk = 0;
				rs.Qj = 0; rs.Qk = 0;
				rs.dest = i->dest;
				rs.A = i->A + i->Vj;

				LOAD_BUFFER.emplace_back(rs);
				break;
			}
			case Opcode::STORE:{
				ROB_ENTRY* b = (ROB_ENTRY*)(i->dest);
				b->addr = i->A + i->Vj;
				b->ready_addr = true;

				
				break;
			}
			case Opcode::AMO: {
				ROB_ENTRY* b = (ROB_ENTRY*)(i->dest);
				b->addr = i->Vj;
				b->ready_addr = true;

				if (i->function != Function::SC_W) {
					RS_ENTRY rs;
					rs.function = i->function;
					rs.opcode = i->opcode;
					rs.in_use = true;

					rs.Vj = 0; rs.Qj = 0;
					rs.Vk = i->Vk; rs.Qk = 0;

					rs.dest = i->dest;
					rs.A = i->Vj;

					LOAD_BUFFER.emplace_back(rs);
				}
				break;
			}
			}
			
			i = ADDR_RS.erase(i);
		}
		else
			++i;
	}

	return Stage_Result::ADDR;
}

bool Tomasulo::find_mem_value_in_ROB(uint32_t start_el, uint32_t addr, bool & valid, int32_t& value)
{
	ROB_ENTRY* start_point = (ROB_ENTRY*)(start_el);
	std::list<ROB_ENTRY>::reverse_iterator it = ROB_queue.rbegin();
	while (it != ROB_queue.rend()) {
		if (start_point == &(*it)) {
			++it;
			while (it != ROB_queue.rend()) {
				if (it->insn.opcode == Opcode::STORE
					||(it->insn.opcode == Opcode::AMO && 
						it->insn.function != Function::LR_W)) {
					if (it->ready_addr) {
						if (addr == it->addr) {
							if (it->ready_value) {
								value = it->mem_value;
								valid = true;
								return true;
							}
							else {
								valid = false;
								return true;
							}
						}
					}
					else {
						valid = false;
						return true;
					}
				}
				++it;
			}
			break;
		}
		++it;
	}
	return false;
}

// return mem_value
int32_t Tomasulo::amo(Function func, int32_t load_value, int32_t src)
{
	switch (func)
	{
	case Function::LR_W:
	{
		return 0;
	}
	case Function::AMOSWAP_W: {
		
		int32_t result = src;
		return result;
	}
	case Function::AMOADD_W:
	{
		int32_t result = load_value + src;
		return result;
	}
	case Function::AMOXOR_W:
	{
		int32_t result = load_value ^ src;
		return result;
	}
	case Function::AMOAND_W:
	{
		int32_t result = load_value & src;
		return result;
	}
	case Function::AMOOR_W:
	{
		int32_t result = load_value | src;
		return result;
	}
	case Function::AMOMIN_W:
	{
		int32_t result = (load_value < src) ? load_value : src;
		return result;
	}
	case Function::AMOMAX_W:
	{
		int32_t result = (load_value > src) ? load_value : src;
		return result;
	}
	case Function::AMOMINU_W:
	{
		uint32_t result = (uint32_t(load_value) < uint32_t(src))
			? uint32_t(load_value) : uint32_t(src);
		return result;
	}
	case Function::AMOMAXU_W:
	{
		uint32_t result = (uint32_t(load_value) > uint32_t(src))
			? uint32_t(load_value) : uint32_t(src);
		return result;
	}
	}
}

int32_t Tomasulo::read_memory(Function func, int32_t addr)
{
	switch (func)
	{
	case Function::LB:
		return memory->read_int(addr, BYTE_SIZE);
	case Function::LH:			
		return memory->read_int(addr, HALFWORD_SIZE);
	case Function::LW:			
		return memory->read_int(addr, WORD_SIZE);
	case Function::LBU:			
		return memory->read_int(addr, BYTE_SIZE, false);
	case Function::LHU:			
		return memory->read_int(addr, HALFWORD_SIZE, false);
	default:
		return memory->read_int(addr, WORD_SIZE);
	}
}

void Tomasulo::write_memory(Function func, int32_t addr, int32_t value)
{
	switch (func)
	{
	case Function::SB:
		memory->write(addr, BYTE_SIZE, (uint32_t*)&value);
		break;
	case Function::SH:
		memory->write(addr, HALFWORD_SIZE, (uint32_t*)&value);
		break;
	case Function::SW:
		memory->write(addr, WORD_SIZE, (uint32_t*)&value);
		break;
	default:
		memory->write(addr, WORD_SIZE, (uint32_t*)&value);
		break;
	}
}

Stage_Result Tomasulo::execute_memory_unit()
{
	// check the load buffer
	bool load{ true };
	if (LOAD_BUFFER.empty()) load = false;

	std::list<RS_ENTRY>::iterator i = LOAD_BUFFER.begin();
	while (i != LOAD_BUFFER.end()) {
		if (i->Qj == 0 && i->Qk == 0) {
			if (i->cycle == 0) {
				// check ROB
				bool valid = false;
				int32_t value = 0;
				bool result = find_mem_value_in_ROB(i->dest, i->A, valid, value);
				if (result) {
					if (valid) {
						i->result = value;
						if (i->opcode == Opcode::AMO) {
							ROB_ENTRY* b = (ROB_ENTRY*)(i->dest);
							b->mem_value = amo(i->function, value, i->Vk);
							b->ready_value = true;
						}
						i->cycle = CACHE_ACCESS_CYCLE;
					}
				}
				else {
					++(i->cycle);
				}
			}
			else if (i->cycle < CACHE_ACCESS_CYCLE) {
				if (++(i->cycle) == CACHE_ACCESS_CYCLE) {
					i->result = read_memory(i->function, i->A);
					if (i->opcode == Opcode::AMO) {
						ROB_ENTRY* b = (ROB_ENTRY*)(i->dest);
						b->mem_value = amo(i->function, i->result, i->Vk);
						b->ready_value = true;
					}
				}
			}
			
		}
		++i;
	}

	// check ROB
	if (ROB_queue.front().insn.opcode == Opcode::STORE
		|| (ROB_queue.front().insn.opcode == Opcode::AMO
			&& ROB_queue.front().insn.function != Function::LR_W)) {
		if (ROB_queue.front().ready_addr && ROB_queue.front().ready_value) {
			if (++(ROB_queue.front().cycle) == 1) {
				write_memory(ROB_queue.front().insn.function,
					ROB_queue.front().addr, ROB_queue.front().mem_value);
                if(ROB_queue.front().insn.function == Function::SC_W){
                      RS_ENTRY rs;
                      fill_RSentry(ROB_queue.front().insn, rs, uint32_t(&ROB_queue.front()));
                      rs.result = 0;
                      rs.cycle = CACHE_ACCESS_CYCLE;
                      LOAD_BUFFER.emplace_back(rs);
			    }
		    }
	    }
    }

	return Stage_Result::MEM;
}

void Tomasulo::get_ALU_RS_completion(std::list<CDB_ENTRY>& cdb)
{
	std::list<RS_ENTRY>::iterator it = ALU_RS.begin();
	while (it != ALU_RS.end()) {
		if (it->cycle >= 1) {
			ROB_ENTRY* b = (ROB_ENTRY*)(it->dest);
			if(b->insn.function != Function::ECALL)
				b->complete = true;
            else
                b->ready_value = true;
			b->value = it->result;
			if (b->rd != 0 && b->insn.function != Function::ECALL) {
				cdb.emplace_back( it->dest, it->result );
			}

			it = ALU_RS.erase(it);
		}
		else
			++it;
	}
}

void Tomasulo::get_MULDIV_RS_completion(std::list<CDB_ENTRY>& cdb)
{
	std::list<RS_ENTRY>::iterator it = MULDIV_RS.begin();
	while (it != MULDIV_RS.end()) {
	    bool erase{ false };
		switch (it->function)
		{
		case Function::MUL:
		case Function::MULH:
		case Function::MULHSU:
		case Function::MULHU: {
			if ((it->cycle) >= MUL_CYCLE) {
				ROB_ENTRY* b = (ROB_ENTRY*)(it->dest);
				b->complete = true;
				b->value = it->result;
				if (b->rd != 0) {
					cdb.emplace_back(it->dest, it->result);
				}
				erase = true;
			}
			break;
		}
		case Function::DIV:
		case Function::DIVU:
		case Function::REM:
		case Function::REMU: {
			if ((it->cycle) >= DIV_CYCLE) {
				ROB_ENTRY* b = (ROB_ENTRY*)(it->dest);
				b->complete = true;
				b->value = it->result;
				if (b->rd != 0) {
					cdb.emplace_back(it->dest, it->result);
				}
				erase = true;
			}
			break;
		}
		}
		if (erase) it = MULDIV_RS.erase(it);
		else ++it;
	}
			
}

void Tomasulo::get_LOAD_BUFFER_completion(std::list<CDB_ENTRY>& cdb)
{
	std::list<RS_ENTRY>::iterator it = LOAD_BUFFER.begin();
	while (it != LOAD_BUFFER.end()) {
		if (it->cycle >= CACHE_ACCESS_CYCLE) {
			ROB_ENTRY* b = (ROB_ENTRY*)(it->dest);
			b->complete = true;
			b->value = it->result;
			if (b->rd != 0) {
				cdb.emplace_back(it->dest, it->result);
			}

			it = LOAD_BUFFER.erase(it);
		}
		else
			++it;
	}
}

void Tomasulo::broadcast(CDB_ENTRY cdb)
{
	// ALU_RS
	std::list<RS_ENTRY>::iterator it = ALU_RS.begin();
	while (it != ALU_RS.end()) {
		if (it->Qj == cdb.nROB) {
			it->Vj = cdb.value;
			it->Qj = 0;
		}
		if (it->Qk == cdb.nROB) {
			it->Vk = cdb.value;
			it->Qk = 0;
		}
	
		++it;
	}
	// MULDIV
	it = MULDIV_RS.begin();
	while (it != MULDIV_RS.end()) {
		if (it->Qj == cdb.nROB) {
			it->Vj = cdb.value;
			it->Qj = 0;
		}
		if (it->Qk == cdb.nROB) {
			it->Vk = cdb.value;
			it->Qk = 0;
		}

		++it;
	}
	// ADDR_RS
	it = ADDR_RS.begin();
	while (it != ADDR_RS.end()) {
		if (it->Qj == cdb.nROB) {
			it->Vj = cdb.value;
			it->Qj = 0;
		}
		if (it->Qk == cdb.nROB) {
			it->Vk = cdb.value;
			it->Qk = 0;
		}

		++it;
	}
	// ROB
	std::list<ROB_ENTRY>::iterator i = ROB_queue.begin();
	while (i != ROB_queue.end()) {
		if (i->src == cdb.nROB
			&&(i->insn.opcode == Opcode::STORE
				|| i->insn.function == Function::SC_W)) {
			i->mem_value = cdb.value;
			i->src = 0;
			i->ready_value = true;
		}
		++i;
	}
}

Stage_Result Tomasulo::write_result()
{
	std::list<CDB_ENTRY> CDB;
	get_ALU_RS_completion(CDB);
	get_MULDIV_RS_completion(CDB);
	get_LOAD_BUFFER_completion(CDB);

	std::list<CDB_ENTRY>::iterator i = CDB.begin();
	while (i != CDB.end()) {
		broadcast(*i);
		++i;
	}

	return Stage_Result::CDB;
}

void Tomasulo::ROB_clear()
{
	instrunction_queue.clear();
	ALU_RS.clear();
	MULDIV_RS.clear();
	ADDR_RS.clear();
	LOAD_BUFFER.clear();
	ROB_queue.clear();
	for (int i = 0; i < 32; ++i)
		register_stat[i].busy = false;
}

Stage_Result Tomasulo::commit(unsigned long long clock)
{
	std::list<ROB_ENTRY>::iterator b = ROB_queue.begin();
	bool clear = false;
	while (b != ROB_queue.end()) {
		if (b->insn.opcode == Opcode::STORE
			|| (b->insn.opcode == Opcode::AMO
				&& b->insn.function != Function::LR_W)) {
			if (b->cycle >= 1 && b->complete) {
				if (b->rd != 0) {
					register_file.gpr[b->rd] = b->value;
					if (register_stat[b->rd].nROB == (uint32_t)(&(*b)))
						register_stat[b->rd].busy = false;
				}
                //std::clog << std::hex << b->insn.fields.pc << std::endl;
                //for(int i = 0; i < 32; ++i)
                //    std::clog << " [" << i << "]:" << std::hex << register_file.gpr[i];
                //std::clog << std::endl;
				b = ROB_queue.erase(b);
			}
			else
				break;
		}
		else {
			if (b->insn.function == Function::ECALL) {
                if(b->ready_value){
				    handle_syscall(register_file, *memory, clock);
				    //register_file.pc = (b->insn.fields.pc + WORD_SIZE);
				    register_file.pc = b->value;

				    if (register_stat[b->rd].nROB == (uint32_t)(&(*b)))
					    register_stat[b->rd].busy = false;
                    //std::clog << std::hex << b->insn.fields.pc << std::endl;
                    //for(int i = 0; i < 32; ++i)
                    //    std::clog << " [" << i << "]:" << std::hex << register_file.gpr[i];
                    //std::clog << std::endl;
				    b = ROB_queue.erase(b);
				    clear = true;
				    break;
			    }
                else
                    break;
            }

			if (b->complete == true) {
				if (b->insn.opcode == Opcode::BRANCH){
					bool predict = b->insn.taken;
					bool result = (b->value > 0)?true:false;
					if (predict != result) {
						if (result)
							register_file.pc = (b->insn.fields.pc + b->insn.fields.imm);
						else
							register_file.pc = (b->insn.fields.pc + WORD_SIZE);
						clear = true;
                        //std::clog << std::hex << b->insn.fields.pc << std::endl;
                        //for(int i = 0; i < 32; ++i)
                        //    std::clog << " [" << i << "]:" << std::hex << register_file.gpr[i];
                        //std::clog << std::endl;
						b = ROB_queue.erase(b);
						break;
					}
				}

				if (b->rd != 0) {
					register_file.gpr[b->rd] = b->value;
					if (register_stat[b->rd].nROB == (uint32_t)(&(*b)))
						register_stat[b->rd].busy = false;
				}
                //std::clog << std::hex << b->insn.fields.pc << std::endl;
                //for(int i = 0; i < 32; ++i)
                //    std::clog << " [" << i << "]:" << std::hex << register_file.gpr[i];
                //std::clog << std::endl;
				b = ROB_queue.erase(b);
			}
			else
				break;
		}
	}

	if (clear) ROB_clear();

	return Stage_Result::COMMIT;
}

void Tomasulo::run()
{
	unsigned long long clock = 1;

	while (true)
	{
		int shut_down = 0;
		uint32_t fetch = 0;

		commit(clock);
		write_result();
		execute_alu();
		execute_muldiv();
		execute_addr_unit();
		execute_memory_unit();
		issue();
		fetch = register_file.pc;
		fetch_n_decode();
        //std::clog << std::hex << fetch;

		++clock;

		//for (int i = 0; i < 32; ++i)
		//	std::clog << " [" << i << "]:" << std::hex << register_file.gpr[i];
		//std::clog << std::endl;
	}
}





