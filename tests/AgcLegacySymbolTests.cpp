#include "libs/libs.h"
#include "loader/symbolDatabase.h"

#include <cstdint>
#include <cstdio>

int main() {
	Loader::SymbolDatabase symbols;
	Libs::InitAll(&symbols);

	const auto* init_symbol = symbols.FindByNid("kW3GLb7QfPg", Loader::SymbolType::Func);
	if (init_symbol == nullptr || init_symbol->vaddr == 0) {
		std::fprintf(stderr, "legacy Agc sceAgcInit import is not registered\n");
		return 1;
	}
	auto init = reinterpret_cast<int(KYTY_SYSV_ABI*)(uint32_t*, uint32_t)>(init_symbol->vaddr);
	uint32_t state = 0x12345678;
	if (init(&state, 8) != 0 || state != 0x12345678) {
		std::fprintf(stderr, "legacy Agc sceAgcInit behavior is incorrect\n");
		return 1;
	}

	const auto* defaults_symbol =
	    symbols.FindByNid("Wi82ArQtAwg", Loader::SymbolType::Func);
	const auto* defaults_v2_symbol =
	    symbols.FindByNid("2JtWUUiYBXs", Loader::SymbolType::Func);
	if (defaults_symbol == nullptr || defaults_symbol->vaddr == 0 ||
	    defaults_v2_symbol == nullptr || defaults_v2_symbol->vaddr == 0) {
		std::fprintf(stderr, "AGC register-default imports are not registered\n");
		return 1;
	}
	auto get_defaults = reinterpret_cast<void*(KYTY_SYSV_ABI*)()>(defaults_symbol->vaddr);
	auto get_defaults_v2 =
	    reinterpret_cast<void*(KYTY_SYSV_ABI*)(uint32_t)>(defaults_v2_symbol->vaddr);
	const auto* defaults = get_defaults();
	if (defaults == nullptr || defaults != get_defaults() ||
	    defaults != get_defaults_v2(0)) {
		std::fprintf(stderr, "legacy AGC register defaults are invalid or unstable\n");
		return 1;
	}

	return 0;
}
