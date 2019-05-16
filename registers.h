#pragma once
#include <stdint.h>

struct RegisterFile {
	int32_t pc{ 0 };
	int32_t gpr[32]{ 0 };
	float fpr[32]{ 0.f };
};


