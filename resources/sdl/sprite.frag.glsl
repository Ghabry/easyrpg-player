#version 460

layout (set = 2, binding = 0) uniform sampler2D u_texture;
layout (location = 0) in vec2 v_coord;
layout (location = 0) out vec4 FragColor;

/*layout(std140, set = 3, binding = 0) uniform UniformBlock {
    float time;
};*/

void main() {
	vec4 texColor = texture(u_texture, v_coord);
	FragColor = texColor;
}
