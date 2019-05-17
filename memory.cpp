#include "memory.h"
#include <string.h>
#include <iostream>

int32_t Memory::read_int(uint32_t vaddr, uint8_t size, bool sigend)
{
	if (base_vaddr <= vaddr && vaddr <= max_vaddr) {
		switch (size)
		{
		case BYTE_SIZE: {
			int8_t b;
			memcpy(&b, &memory[vaddr - base_vaddr], BYTE_SIZE);
			if (sigend)
				return b;
			else
				return uint8_t(b);
		}
		case HALFWORD_SIZE: {
			int16_t h;
			memcpy(&h, &memory[vaddr - base_vaddr], HALFWORD_SIZE);
			if (sigend)
				return h;
			else
				return uint16_t(h);
		}
		case WORD_SIZE: {
			int32_t w;
			memcpy(&w, &memory[vaddr - base_vaddr], WORD_SIZE);
			if (sigend)
				return w;
			else
				return uint32_t(w);
		}
		}
	}
	else if ((0xffffffff - STACK_SIZE) <= vaddr && vaddr <= 0xffffffff) {
		switch (size)
		{
		case BYTE_SIZE: {
			int8_t b;
			memcpy(&b
				, &stack[vaddr - STACK_OFFSET], BYTE_SIZE);
			if (sigend)
				return b;
			else
				return uint8_t(b);
		}
		case HALFWORD_SIZE: {
			int16_t h;
			memcpy(&h
				, &stack[vaddr - STACK_OFFSET], HALFWORD_SIZE);
			if (sigend)
				return h;
			else
				return uint16_t(h);
		}
		case WORD_SIZE: {
			int32_t w;
			memcpy(&w
				, &stack[vaddr - STACK_OFFSET], WORD_SIZE);
			if (sigend)
				return w;
			else
				return uint32_t(w);
		}
		}
	}
	else if (0 <= vaddr && vaddr <= TLS_SIZE) {
		switch (size)
		{
		case BYTE_SIZE: {
			int8_t b;
			memcpy(&b
				, &tls[vaddr], BYTE_SIZE);
			if (sigend)
				return b;
			else
				return uint8_t(b);
		}
		case HALFWORD_SIZE: {
			int16_t h;
			memcpy(&h
				, &tls[vaddr], HALFWORD_SIZE);
			if (sigend)
				return h;
			else
				return uint16_t(h);
		}
		case WORD_SIZE: {
			int32_t w;
			memcpy(&w
				, &tls[vaddr], WORD_SIZE);
			if (sigend)
				return w;
			else
				return uint32_t(w);
		}
		}

	}
	
	std::clog << "invalid memory access [" << std::hex << vaddr << "]" << std::endl;
	exit(1);
	return 0;
}

float Memory::read_float(uint32_t vaddr)
{
	
	if (base_vaddr <= vaddr && vaddr <= max_vaddr) {
		float f;
		memcpy(&f, &memory[vaddr - base_vaddr], WORD_SIZE);
	
		return f;
	}
	else if ((0xffffffff - STACK_SIZE) <= vaddr && vaddr <= 0xffffffff) {
		float f;
		memcpy(&f
			, &stack[vaddr - STACK_OFFSET], WORD_SIZE);

		return f;
		
	}
	else if (0 <= vaddr && vaddr <= TLS_SIZE) {
		float f;
		memcpy(&f
			, &tls[vaddr], WORD_SIZE);

		return f;

	}

	std::clog << "invalid memory access [" << std::hex<< vaddr << "]" << std::endl;
	exit(1);
	return 0.0f;
}

void Memory::write(uint32_t vaddr, uint8_t size, uint32_t* data)
{
	if (base_vaddr <= vaddr && vaddr <= max_vaddr) {
		if(size == BYTE_SIZE || size == HALFWORD_SIZE)
            memcpy(&memory[vaddr - base_vaddr]
			    , (const void*)(data), size);
		else 
            memcpy(&memory[vaddr - base_vaddr]
			    , (const void*)(data), size);

        return;
	}
	else if ((0xffffffff - STACK_SIZE) <= vaddr && vaddr <= 0xffffffff) {
		if(size == BYTE_SIZE || size == HALFWORD_SIZE)
		    memcpy(&stack[vaddr - STACK_OFFSET]
			    , (const void*)(data), size);
		else
		    memcpy(&stack[vaddr - STACK_OFFSET]
			    , (const void*)(data), size);

        return;

	}
	else if (0 <= vaddr && vaddr <= TLS_SIZE) {
		if(size == BYTE_SIZE || size == HALFWORD_SIZE)
		    memcpy(&tls[vaddr]
			    , (const void*)(data), size);
		else
		    memcpy(&tls[vaddr]
			    , (const void*)(data), size);
        return;

	}

	std::clog << "invalid memory access [" << std::hex << vaddr << "]" << std::endl;
	exit(1);
}

char* Memory::get_ptr(uint32_t vaddr)
{
	if (base_vaddr <= vaddr && vaddr <= max_vaddr) {
		return &memory[vaddr - base_vaddr];
	}
	else if ((0xffffffff - STACK_SIZE) <= vaddr && vaddr <= 0xffffffff) {
		return &stack[vaddr - STACK_OFFSET];
	}
	else if (0 <= vaddr && vaddr <= TLS_SIZE) {
		return &tls[vaddr];
	}

	std::clog << "invalid memory access [" << std::hex << vaddr << "]" << std::endl;
	exit(1);
	return 0;
}

