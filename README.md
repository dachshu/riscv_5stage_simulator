# riscV 5stage simulator

Implemented RiscV CPU simulator by [Instruction Set Manual](https://riscv.org/wp-content/uploads/2017/05/riscv-spec-v2.2.pdf). It has two arguments a type of scheduling and a statically linked elf(Executable and Linkable Format) file. I build the sample codes using [riscv-gnu-toolchain](https://github.com/riscv/riscv-gnu-toolchain). The simulator parses the elf file by [this](http://www.skyfree.org/linux/references/ELF_Format.pdf) and initializes text, initialized data and uninitialized data memory. Also it sets a entry point and intializes stack memory by [Linux stack frame](https://refspecs.linuxfoundation.org/ELF/zSeries/lzsabi0_zSeries/x895.html). And setting PC and SP(GPR) registers. The sheduling type is 0-3 integer(0: in-order 5-stage, 1: tomasulo, 2: tomasulo + 2way super scalar, 3: tomoasulo + 2way super scalar + 2bit branch prediction).

- reference
[1] https://github.com/riscv/riscv-pk
[2] https://github.com/djanderson/riscv-5stage-simulator
