#include <iostream>
#include "elf.h"
#include "memory.h"
#include "pipeline.h"
#include "tomasulo.h"
#include "tomasulo_2.h"

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
	//load_elf("hello-riscv-dbg", entry_point, base_vaddr, max_vaddr, memory
	//	, stack, sp);
	load_elf(argv[2], entry_point, base_vaddr, max_vaddr, memory
		, stack, sp);

	if (!memory) {
		clog << "memory empty!!!" << endl;
		return 0;
	}
	
	std::clog << "entry point : " << entry_point << std::endl;

	Memory mem{ entry_point, base_vaddr, max_vaddr, memory, stack };
    
    // 0: in-order 5-stage
    // 1: tomasulo
    // 2: tomasulo + 2way
    // 3: tomasulo + 2way + 2bit
    switch(*argv[1]){
        case '0':{ 
                    Pipeline pipeline{ &mem, entry_point, sp };
                    pipeline.run();
                    break;
               }
        case '1':{
                    Tomasulo pipeline{ &mem, entry_point, sp };
                    pipeline.run();
                   break;
               }
        case '2':{
                    Tomasulo_Two pipeline{ &mem, entry_point, sp };
                    pipeline.run();
                    break;
               }
        case '3':{
                    Tomasulo_Two pipeline{ &mem, entry_point, sp, true };
                    pipeline.run();
                    break;
               }
        default:{
                    Pipeline pipeline{ &mem, entry_point, sp };
                    pipeline.run();
                    break;
                }
    }
	//Pipeline pipeline{ &mem, entry_point, sp };
	//Tomasulo pipeline{ &mem, entry_point, sp };
	//Tomasulo_Two pipeline{ &mem, entry_point, sp, true };
    //pipeline.run();
		 

	if (memory)
		delete memory;
	if (stack)
		delete stack;
}
