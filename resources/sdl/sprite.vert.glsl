#version 460

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_coord;

layout (location = 0) out vec2 v_coord;

// SpriteUniform
layout (set = 1, binding = 0) uniform TransformBlock {
	vec2 u_position;      // e.g., x: 50, y: 100
	vec2 u_size;          // e.g., w: 16, h: 16
	vec2 u_screen_size;   // e.g., w: 320, h: 320
};

void main() {
	// Position the sprite and ensure that the image size is in the correct
	// proportion to the game resolution.
	vec2 pixel_pos = (a_position.xy * u_size) + u_position;
	float ndc_x = (pixel_pos.x / u_screen_size.x) * 2.0 - 1.0;
	float ndc_y = 1.0 - (pixel_pos.y / u_screen_size.y) * 2.0;

	gl_Position = vec4(ndc_x, ndc_y, 0.0, 1.0);

	v_coord = a_coord;
}
