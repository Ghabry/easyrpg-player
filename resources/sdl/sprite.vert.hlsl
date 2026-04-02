struct VSInput {
	float3 position : TEXCOORD0;
	float2 texCoord : TEXCOORD1;
};

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
};

struct SpriteUniforms {
	float2 position;
	float2 origin;
	float2 tex_size;
	float2 dst_size;
	float4 src_rect; // x, y, w, h
	float2 screen_size;
	float angle;
};

cbuffer UniformBlock : register(b0, space1) {
	SpriteUniforms ubo;
};

VSOutput main(VSInput input) {
	VSOutput output;

	// Position the sprite and ensure that the image size is in the correct
	// proportion to the game resolution.
	// Scaled by dst_size.
	float2 local_pos = (input.position.xy * ubo.dst_size) - ubo.origin;
	float2 pixel_pos = local_pos + ubo.position;

	float ndc_x = (pixel_pos.x / ubo.screen_size.x) * 2.0 - 1.0;
	float ndc_y = 1.0 - (pixel_pos.y / ubo.screen_size.y) * 2.0;

	output.position = float4(ndc_x, ndc_y, 0.0, 1.0);

	// Apply src_rect to texture coordinates
	float2 uv_min = ubo.src_rect.xy / ubo.tex_size;
	float2 uv_size = ubo.src_rect.zw / ubo.tex_size;

	output.texCoord = uv_min + input.texCoord * uv_size;

	return output;
}
