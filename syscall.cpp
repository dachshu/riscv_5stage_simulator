#define _CRT_SECURE_NO_WARNINGS

#include "syscall.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/utsname.h>


using namespace std;

uint32_t sys_bark(long a0) {
	return a0;
}

int sys_openat(long dirfd, long name, long flags, long mode, Memory& memory)
{
	return openat(dirfd, memory.get_ptr(name), flags, mode);
}


int sys_uname(long buf, Memory& memory)
{
	return uname((struct utsname*)memory.get_ptr(buf));
}

int sys_getuid()
{
    return 0;
}

int sys_readlinkat(long dirfd, long pathname, long buf, long bufsize, Memory& memory)
{
    return readlinkat(dirfd, memory.get_ptr(pathname), memory.get_ptr(buf), bufsize);
}

int sys_fstat(long fd, long buf, Memory& memory)
{
    return fstat(fd, (struct stat*)memory.get_ptr(buf));
}

int sys_write(long fd, long buf, long count, Memory& memory){
    return write(fd, memory.get_ptr(buf), count);
}

long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, unsigned long n, Memory& memory, unsigned long long clock)
{
	switch (n)
	{
    case SYS_geteuid:
    case SYS_getuid:
    case SYS_getegid:
    case SYS_getgid:
        return sys_getuid();
	case SYS_brk:
		return sys_bark(a0);
	case SYS_uname:
		return sys_uname(a0, memory);
	case SYS_openat:
		return sys_openat(a0, a1, a2, a3, memory);
    case SYS_readlinkat:
        return sys_readlinkat(a0, a1, a2, a3, memory);
    case SYS_fstat:
        return sys_fstat(a0, a1, memory);
    case SYS_write:
        return sys_write(a0, a1, a2, memory);
    case SYS_exit:
    case SYS_exit_group:
        std::clog << "bye~!" << std::endl;
        std::clog << std::dec << "[ clock ] " << clock << std::endl;
        exit(0);
    default:
        std::clog << "not defined syscall " << std::dec << n << std::endl;
		exit(1);
        break;
	}
	return 0;
}


void handle_syscall(RegisterFile& register_file, Memory& memory, unsigned long long clock)
{
	register_file.gpr[10] = do_syscall(register_file.gpr[10]
		, register_file.gpr[11], register_file.gpr[12], register_file.gpr[13],
		register_file.gpr[14], register_file.gpr[15], register_file.gpr[17], memory, clock);
    std::clog << "sys call num: " << std::dec << register_file.gpr[17] << std::endl;
}

