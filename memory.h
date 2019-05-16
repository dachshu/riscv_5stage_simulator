#pragma once
#include <stdint.h>
#include "consts.h"

class Memory {
	uint32_t entry_point;
	uint32_t base_vaddr;
	uint32_t max_vaddr;

	char* memory{ nullptr };
	char* stack{ nullptr };
	char* tls{ nullptr };


public:
	Memory(uint32_t entry, uint32_t base, uint32_t max, char* mem, char* stk) :
		entry_point(entry), base_vaddr(base), max_vaddr(max), memory(mem), stack(stk)
	{
		tls = new char[TLS_SIZE]();
	}
	~Memory() {
		if (tls) delete tls;
	}

	int32_t read_int(uint32_t vaddr, uint8_t size, bool sigend = true);
	float read_float(uint32_t vaddr);
	void write(uint32_t vaddr, uint8_t size, uint32_t* data);
	char* get_ptr(uint32_t vaddr);
};
