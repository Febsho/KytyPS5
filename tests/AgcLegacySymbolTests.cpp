#include "common/emulatorConfig.h"
#include "graphics/guest_gpu/pm4.h"
#include "graphics/shader/shader.h"
#include "libs/libs.h"
#include "loader/symbolDatabase.h"

#include <array>
#include <cstdint>
#include <cstdio>

int main() {
	Config::Initialize();
	Loader::SymbolDatabase symbols;
	Libs::InitAll(&symbols);

	const auto* init_symbol = symbols.FindByNid("kW3GLb7QfPg", Loader::SymbolType::Func);
	if (init_symbol == nullptr || init_symbol->vaddr == 0) {
		std::fprintf(stderr, "legacy Agc sceAgcInit import is not registered\n");
		Config::Shutdown();
		return 1;
	}
	auto init = reinterpret_cast<int(KYTY_SYSV_ABI*)(uint32_t*, uint32_t)>(init_symbol->vaddr);
	uint32_t state = 0x12345678;
	if (init(&state, 8) != 0 || state != 0x12345678) {
		std::fprintf(stderr, "legacy Agc sceAgcInit behavior is incorrect\n");
		Config::Shutdown();
		return 1;
	}

	const auto* defaults_symbol =
	    symbols.FindByNid("Wi82ArQtAwg", Loader::SymbolType::Func);
	const auto* defaults_v2_symbol =
	    symbols.FindByNid("2JtWUUiYBXs", Loader::SymbolType::Func);
	if (defaults_symbol == nullptr || defaults_symbol->vaddr == 0 ||
	    defaults_v2_symbol == nullptr || defaults_v2_symbol->vaddr == 0) {
		std::fprintf(stderr, "AGC register-default imports are not registered\n");
		Config::Shutdown();
		return 1;
	}
	auto get_defaults = reinterpret_cast<void*(KYTY_SYSV_ABI*)()>(defaults_symbol->vaddr);
	auto get_defaults_v2 =
	    reinterpret_cast<void*(KYTY_SYSV_ABI*)(uint32_t)>(defaults_v2_symbol->vaddr);
	const auto* defaults = get_defaults();
	if (defaults == nullptr || defaults != get_defaults() ||
	    defaults != get_defaults_v2(0)) {
		std::fprintf(stderr, "legacy AGC register defaults are invalid or unstable\n");
		Config::Shutdown();
		return 1;
	}

	const auto* link_symbol = symbols.FindByNid("MqAdbRMdNz4", Loader::SymbolType::Func);
	if (link_symbol == nullptr || link_symbol->vaddr == 0) {
		std::fprintf(stderr, "legacy Agc sceAgcLinkShaders import is not registered\n");
		Config::Shutdown();
		return 1;
	}
	auto link_shaders = reinterpret_cast<int(KYTY_SYSV_ABI*)(
	    Libs::Graphics::ShaderRegister*, Libs::Graphics::ShaderRegister*, const void*,
	    const Libs::Graphics::Shader*, const Libs::Graphics::Shader*, uint32_t)>(link_symbol->vaddr);
	Libs::Graphics::ShaderSpecialRegs special {};
	special.ge_cntl.offset = Libs::Graphics::Pm4::GE_CNTL;
	special.ge_user_vgpr_en.offset = Libs::Graphics::Pm4::GE_USER_VGPR_EN;
	special.vgt_shader_stages_en.offset = Libs::Graphics::Pm4::VGT_SHADER_STAGES_EN;
	Libs::Graphics::Shader vertex {};
	vertex.type = static_cast<uint8_t>(Libs::Graphics::Prospero::ShaderBinaryType::kGs);
	vertex.specials = &special;
	std::array<Libs::Graphics::ShaderRegister, 34> context {};
	std::array<Libs::Graphics::ShaderRegister, 3> primitive {};
	if (link_shaders(context.data(), primitive.data(), nullptr, &vertex, nullptr, 3) != 0 ||
	    primitive[0].offset != Libs::Graphics::Pm4::GE_CNTL ||
	    primitive[1].offset != Libs::Graphics::Pm4::GE_USER_VGPR_EN ||
	    primitive[2].offset != Libs::Graphics::Pm4::VGT_PRIMITIVE_TYPE || primitive[2].value != 3 ||
	    context[0].offset != Libs::Graphics::Pm4::VGT_SHADER_STAGES_EN ||
	    context[1].offset != Libs::Graphics::Pm4::VGT_GS_OUT_PRIM_TYPE ||
	    context[2].offset != Libs::Graphics::Pm4::SPI_PS_INPUT_CNTL_0 ||
	    context[33].offset != Libs::Graphics::Pm4::SPI_PS_INPUT_CNTL_0 + 31) {
		std::fprintf(stderr, "legacy Agc sceAgcLinkShaders register output is incorrect\n");
		Config::Shutdown();
		return 1;
	}

	Config::Shutdown();
	return 0;
}
