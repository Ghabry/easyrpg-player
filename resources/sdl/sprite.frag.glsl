#version 460

layout (set = 2, binding = 0) uniform sampler2D u_texture;
layout (location = 0) in vec2 v_coord;
layout (location = 0) out vec4 FragColor;

layout(std140, set = 3, binding = 0) uniform UniformBlock {
	vec4 u_tone;
	vec4 u_flash;
	float u_opacity_top;
	float u_opacity_bottom;
	float u_waver_depth;
	float u_waver_phase;
	float u_blend_mode;
};

void main() {
	vec4 texColor = texture(u_texture, v_coord);
	FragColor = texColor;
}
