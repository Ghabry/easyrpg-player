struct VSInput {
	float3 position : TEXCOORD0;
	float2 texCoord : TEXCOORD1;
};

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
	float2 localCoord : TEXCOORD1;
};

struct SpriteUniforms {
	float2 position;
	float2 origin;
	float2 tex_size;
	float2 dst_size;
	float4 src_rect; // x, y, w, h
	float2 screen_size;
	float angle;
	float tiling;
};

cbuffer UniformBlock : register(b0, space1) {
	SpriteUniforms ubo;
};

VSOutput main(VSInput input) {
	VSOutput output;

	// Setup rotation matrix
	float c = cos(ubo.angle);
	float s = sin(ubo.angle);
	float2x2 rot = float2x2(c, -s, s, c);

	// Position the sprite and ensure that the image size is in the correct
	// proportion to the game resolution.
	float2 local_pos = (input.position.xy * ubo.dst_size * ubo.tiling) - ubo.origin;
	float2 rotated_pos = mul(rot, local_pos);
	float2 pixel_pos = rotated_pos + ubo.position;

	// normalize coordinates
	float ndc_x = (pixel_pos.x / ubo.screen_size.x) * 2.0 - 1.0;
	float ndc_y = 1.0 - (pixel_pos.y / ubo.screen_size.y) * 2.0;

	output.position = float4(ndc_x, ndc_y, 0.0, 1.0);

	// Apply src_rect to texture coordinates
	float2 uv_min = ubo.src_rect.xy / ubo.tex_size;
	float2 uv_size = ubo.src_rect.zw / ubo.tex_size;

	output.texCoord = uv_min + (input.texCoord * ubo.tiling) * uv_size;
	output.localCoord = input.texCoord;

	return output;
}
