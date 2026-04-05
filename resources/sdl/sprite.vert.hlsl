// SPDX-License-Identifier: MIT-0

struct VSInput {
	float3 position : TEXCOORD0;
	float2 texCoord : TEXCOORD1;
};

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
	float2 localCoord : TEXCOORD1;
	float4 uv_rect : TEXCOORD2;
};

struct SpriteUniforms {
	float4x4 proj_matrix;
	float4x4 model_matrix;
	float4 uv_rect; // x, y, width, height
};

cbuffer UniformBlock : register(b0, space1) {
	SpriteUniforms ubo;
};

VSOutput main(VSInput input) {
	VSOutput output;

	float4 pos = float4(input.position, 1.0f);

	float4 world_pos = mul(ubo.model_matrix, pos);
	output.position = mul(ubo.proj_matrix, world_pos);

	output.texCoord = input.texCoord * ubo.uv_rect.zw + ubo.uv_rect.xy;

	output.localCoord = input.texCoord;
	output.uv_rect = ubo.uv_rect;

	return output;
}
