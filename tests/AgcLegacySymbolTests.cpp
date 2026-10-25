#include "common/emulatorConfig.h"
#include "graphics/guest_gpu/pm4.h"
#include "graphics/shader/shader.h"
#include "libs/agc.h"
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

	const auto* create_shader_symbol =
	    symbols.FindByNid("f3dg2CSgRKY", Loader::SymbolType::Func);
	if (create_shader_symbol == nullptr || create_shader_symbol->vaddr == 0) {
		std::fprintf(stderr, "legacy Agc sceAgcCreateShader import is not registered\n");
		Config::Shutdown();
		return 1;
	}
	Config::ConfigOptions debug_config;
	debug_config.graphics_debug_dump_enabled = true;
	Config::Load(debug_config);
	Libs::Graphics::ShaderInit();
	alignas(256) std::array<uint32_t, 1> shader_code {};
	Libs::Graphics::Shader shader {};
	shader.file_header = 0x34333231;
	shader.version     = 0x18;
	shader.shader_size = sizeof(shader_code);
	shader.type = static_cast<uint8_t>(Libs::Graphics::Prospero::ShaderBinaryType::kFs);
	Libs::Graphics::Shader* created_shader = nullptr;
	auto create_shader = reinterpret_cast<int(KYTY_SYSV_ABI*)(
	    Libs::Graphics::Shader**, void*, const volatile void*)>(create_shader_symbol->vaddr);
	if (create_shader(&created_shader, &shader, shader_code.data()) != 0 ||
	    created_shader != &shader) {
		std::fprintf(stderr,
		             "Agc sceAgcCreateShader failed with optional special registers absent\n");
		Config::Shutdown();
		return 1;
	}

	const auto* set_sh_registers_size_symbol =
	    symbols.FindByNid("yUBESvCCJ4I", Loader::SymbolType::Func);
	const auto* set_uc_registers_indirect_size_symbol =
	    symbols.FindByNid("UQGTw4xRlcM", Loader::SymbolType::Func);
	const auto* acb_dispatch_indirect_size_symbol =
	    symbols.FindByNid("PxKWV2fVAps", Loader::SymbolType::Func);
	if (set_sh_registers_size_symbol == nullptr ||
	    set_uc_registers_indirect_size_symbol == nullptr ||
	    acb_dispatch_indirect_size_symbol == nullptr) {
		std::fprintf(stderr, "AGC command-buffer size imports are not registered\n");
		Config::Shutdown();
		return 1;
	}
	auto get_set_sh_registers_size = reinterpret_cast<uint32_t(KYTY_SYSV_ABI*)(uint32_t)>(
	    set_sh_registers_size_symbol->vaddr);
	auto get_set_uc_registers_indirect_size =
	    reinterpret_cast<uint64_t(KYTY_SYSV_ABI*)()>(
	        set_uc_registers_indirect_size_symbol->vaddr);
	auto get_acb_dispatch_indirect_size = reinterpret_cast<uint32_t(KYTY_SYSV_ABI*)()>(
	    acb_dispatch_indirect_size_symbol->vaddr);
	if (get_set_sh_registers_size(0) != 0 || get_set_sh_registers_size(1) != 12 ||
	    get_set_sh_registers_size(3) != 36 ||
	    get_set_uc_registers_indirect_size() != 20 ||
	    get_acb_dispatch_indirect_size() != 16) {
		std::fprintf(stderr, "AGC command-buffer size query results are incorrect\n");
		Config::Shutdown();
		return 1;
	}

	const auto* wait_size_symbol =
	    symbols.FindByNid("0MtUJ3BpGhE", Loader::SymbolType::Func);
	const auto* wait_symbol = symbols.FindByNid("u8BkdHb1+Po", Loader::SymbolType::Func);
	const auto* get_tf_ring_symbol =
	    symbols.FindByNid("e-YMQ+2tj9M", Loader::SymbolType::Func);
	const auto* set_tf_ring_symbol =
	    symbols.FindByNid("XlNp7jzGiPo", Loader::SymbolType::Func);
	if (wait_size_symbol == nullptr || wait_symbol == nullptr || get_tf_ring_symbol == nullptr ||
	    set_tf_ring_symbol == nullptr) {
		std::fprintf(stderr, "legacy AgcDriver imports are not registered\n");
		Config::Shutdown();
		return 1;
	}
	auto get_wait_size = reinterpret_cast<uint32_t(KYTY_SYSV_ABI*)()>(wait_size_symbol->vaddr);
	auto wait_until_safe = reinterpret_cast<uint32_t(KYTY_SYSV_ABI*)(uint32_t**, uint32_t,
	                                                                  uint32_t, uint32_t, int)>(
	    wait_symbol->vaddr);
	std::array<uint32_t, 4> rendering_wait {};
	auto* rendering_wait_cursor = rendering_wait.data();
	if (get_wait_size() != rendering_wait.size() ||
	    wait_until_safe(&rendering_wait_cursor, rendering_wait.size(), 0, 7, 2) != 0 ||
	    rendering_wait_cursor != rendering_wait.data() + rendering_wait.size() ||
	    rendering_wait[0] != 0xc0021018u || rendering_wait[1] != 7 || rendering_wait[2] != 2 ||
	    rendering_wait[3] != 0) {
		std::fprintf(stderr, "legacy AgcDriver rendering-wait packet is incorrect\n");
		Config::Shutdown();
		return 1;
	}
	auto get_tf_ring = reinterpret_cast<int(KYTY_SYSV_ABI*)(uintptr_t*, uint32_t*)>(
	    get_tf_ring_symbol->vaddr);
	auto set_tf_ring = reinterpret_cast<int(KYTY_SYSV_ABI*)(const volatile void*, uint32_t)>(
	    set_tf_ring_symbol->vaddr);
	uintptr_t tf_ring_base = 0;
	uint32_t tf_ring_size = 0;
	std::array<uint32_t, 4> tf_ring {};
	if (get_tf_ring(&tf_ring_base, &tf_ring_size) != 0 || tf_ring_base != 0xff0000000ull ||
	    tf_ring_size != 0x20000 || set_tf_ring(tf_ring.data(), sizeof(tf_ring)) != 0 ||
	    get_tf_ring(&tf_ring_base, &tf_ring_size) != 0 ||
	    tf_ring_base != reinterpret_cast<uintptr_t>(tf_ring.data()) ||
	    tf_ring_size != sizeof(tf_ring)) {
		std::fprintf(stderr, "legacy AgcDriver tessellation factor ring query is incorrect\n");
		Config::Shutdown();
		return 1;
	}

	Config::Shutdown();
	return 0;
}
