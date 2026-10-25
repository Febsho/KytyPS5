#include "graphics/shader/shader.h"

#include <cstdio>

int main() {
	Libs::Graphics::ShaderPixelInputInfo info {};
	info.input_num = 1;
	info.interpolator_settings[0] = 0x00000400u;
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
