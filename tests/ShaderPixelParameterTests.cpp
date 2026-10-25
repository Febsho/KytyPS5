#include "graphics/shader/shader.h"
#include "graphics/guest_gpu/pm4.h"

#include <cstdio>

int main() {
	Libs::Graphics::ShaderPixelInputInfo info {};
	info.input_num = 1;
	info.interpolator_settings[0] = 0x00000400u;
	info.point_sprite_enable = true;
	if (Libs::Graphics::ShaderPixelParameterUsesPointSprite(info, 0)) {
		std::fprintf(stderr, "global point-sprite enable overrode an unmarked input\n");
		return 1;
	}
	info.interpolator_settings[0] |=
	    Libs::Graphics::Pm4::SPI_PS_INPUT_CNTL_0_PT_SPRITE_TEX_MASK <<
	    Libs::Graphics::Pm4::SPI_PS_INPUT_CNTL_0_PT_SPRITE_TEX_SHIFT;
	if (!Libs::Graphics::ShaderPixelParameterUsesPointSprite(info, 0)) {
		std::fprintf(stderr, "marked input did not combine with global point-sprite enable\n");
		return 1;
	}
	info.point_sprite_enable = false;
	if (Libs::Graphics::ShaderPixelParameterUsesPointSprite(info, 0)) {
		std::fprintf(stderr, "per-input point-sprite mark ignored disabled global control\n");
		return 1;
	}
	info.point_sprite_enable = true;
	info.interpolator_settings[0] &=
	    ~(Libs::Graphics::Pm4::SPI_PS_INPUT_CNTL_0_PT_SPRITE_TEX_MASK <<
	      Libs::Graphics::Pm4::SPI_PS_INPUT_CNTL_0_PT_SPRITE_TEX_SHIFT);
	if (Libs::Graphics::ShaderPixelParameterIsFlat(info, 0)) {
		std::fprintf(stderr, "per-input flat shade ignored disabled global control\n");
		return 1;
	}

	info.flat_shade_enable = true;
	if (!Libs::Graphics::ShaderPixelParameterIsFlat(info, 0)) {
		std::fprintf(stderr, "global and per-input flat shade controls were not combined\n");
		return 1;
	}

	info.custom_interpolation_mask = 1;
	if (Libs::Graphics::ShaderPixelParameterIsFlat(info, 0)) {
		std::fprintf(stderr, "custom interpolation was treated as flat shade\n");
		return 1;
	}

	return 0;
}
