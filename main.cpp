#include <iostream>
#include "elf.h"
#include "memory.h"
#include "pipeline.h"
#include <iostream>

using namespace std;

char* memory;
char* stack;

int main(int argc, char* argv[])
{
	uint32_t entry_point;
	uint32_t base_vaddr;
	uint32_t max_vaddr;
	uint32_t sp;


	//load_elf(argv[1], entry_point, base_vaddr, max_vaddr, memory, stack, sp);
	load_elf("hello-riscv-dbg", entry_point, base_vaddr, max_vaddr, memory
		, stack, sp);

	if (!memory) {
		clog << "memory empty!!!" << endl;
		return 0;
	}
	
	std::clog << "entry point : " << entry_point << std::endl;

	Memory mem{ entry_point, base_vaddr, max_vaddr, memory, stack };
	Pipeline pipeline{ &mem, entry_point, sp };
	pipeline.run();
		 

	if (memory)
		delete memory;
	if (stack)
		delete stack;
}
