#include "elf.h"
#include "consts.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string.h>
#include <memory.h>


using namespace std;

struct Aux {
	uint32_t key;
	uint32_t value;
};

void load_elf(const char* fn
	, uint32_t &entry_point, uint32_t &base_vaddr, uint32_t& max_addr, char* &memory
	, char* &stack, uint32_t &sp)
{
	ifstream in{ fn, ios::binary };

	Elf32_Fhdr fh;
	
	if (in.is_open()) {
		in.read((char*)(&fh), sizeof(Elf32_Fhdr));
	}
	else {
		cout << "not found elf file!" << endl;
		return;
	}

	vector<Elf32_Phdr> phdr;
	uintptr_t max_vaddr = 0;
	uintptr_t min_vaddr = 0xffffffff;
	for (int i = 0; i < fh.e_phnum; ++i) {
		Elf32_Phdr ph;
		in.read((char*)(&ph), sizeof(Elf32_Phdr));
		if (ph.p_type == PT_LOAD && ph.p_memsz) {
			phdr.emplace_back(ph);
			if (ph.p_vaddr + ph.p_memsz > max_vaddr)
				max_vaddr = ph.p_vaddr + ph.p_memsz;
			if (ph.p_vaddr < min_vaddr)
				min_vaddr = ph.p_vaddr;
		}
	}

	memory = new char[max_vaddr - min_vaddr + REMAIN_SIZE]();
	
	for (auto& ph : phdr) {
		in.seekg(ph.p_offset, ios::beg);
		in.read(&memory[ph.p_vaddr - min_vaddr], ph.p_filesz);
	}
	
	base_vaddr = min_vaddr;
	max_addr = max_vaddr + REMAIN_SIZE;
	entry_point = fh.e_entry;

	uint32_t phdrs[128] = {};
	//size_t phdr_size = sizeof(phdrs);
	size_t phdr_cp_size = fh.e_phnum * sizeof(Elf32_Phdr);
	in.seekg(fh.e_phoff, ios::beg);
	in.read((char*)phdrs, phdr_cp_size);

	sp = 0x0;
	stack = new char[STACK_SIZE + 1] {};

	size_t stack_top = sp - phdr_cp_size;
	memcpy(&(stack[stack_top - STACK_OFFSET]), phdrs, phdr_cp_size);
	size_t sp_phdr = stack_top;

	size_t len = strlen(fn) + 1;
	stack_top -= len;
	memcpy(&(stack[stack_top - STACK_OFFSET]), fn, len);
	size_t argc = 1;
	size_t argv_ptr = stack_top;

	const char* envp[] = {""};
	size_t envc = sizeof(envp) / sizeof(envp[0]);
	envc = 0;		// no env
	for (unsigned int i = 0; i < envc; ++i) {
		len = strlen(envp[i]) + 1;
		stack_top -= len;
		memcpy(&(stack[stack_top - STACK_OFFSET]), envp[i], len);
		envp[i] = (char*)(stack_top);
	}

	stack_top &= -4;
	
	Aux aux[] = {
	  {AT_ENTRY, fh.e_entry},
	  {AT_PHNUM, fh.e_phnum},
	  {AT_PHENT, sizeof(Elf32_Phdr)},
	  {AT_PHDR, sp_phdr},
	  {AT_PAGESZ, 0},
	  {AT_SECURE, 0},
	  {AT_RANDOM, stack_top},
	  {AT_NULL, 0}
	};

	size_t naux = sizeof(aux) / sizeof(aux[0]);
	stack_top -= (1 + argc + 1 + envc + 1 + 2 * naux) * sizeof(uintptr_t);
	stack_top &= -16;
	uint32_t st = stack_top;
	memcpy(&(stack[st - STACK_OFFSET]), &argc, sizeof(uintptr_t));
	st += sizeof(uintptr_t);
	memcpy(&(stack[st - STACK_OFFSET]), &argv_ptr, sizeof(uintptr_t));
	st += sizeof(uintptr_t);
	int zero = 0;
	memcpy(&(stack[st - STACK_OFFSET]), &zero, sizeof(uintptr_t));
	st += sizeof(uintptr_t);
	for (unsigned int i = 0; i < envc; ++i) {
		memcpy(&(stack[st - STACK_OFFSET]), &envp[i], sizeof(uintptr_t));
		st += sizeof(uintptr_t);
	}
	memcpy(&(stack[st - STACK_OFFSET]), &zero, sizeof(uintptr_t));
	st += sizeof(uintptr_t);

	for (unsigned int i = 0; i < naux; ++i) {
		memcpy(&(stack[st - STACK_OFFSET]), &(aux[i].key), sizeof(uintptr_t));
		st += sizeof(uintptr_t);
		memcpy(&(stack[st - STACK_OFFSET]), &(aux[i].value), sizeof(uintptr_t));
		st += sizeof(uintptr_t);
	}

	sp = stack_top;
}

