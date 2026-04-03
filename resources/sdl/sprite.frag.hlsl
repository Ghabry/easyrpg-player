Texture2D<float4> main_tex : register(t0, space2);
SamplerState tex_sampler : register(s0, space2);

struct SpriteUniforms {
	float4 tone;
	float4 flash;
	float4 opacity; // top, bottom, split
	float waver_depth;
	float waver_phase;
	float blend_mode;
};

cbuffer UniformBlock : register(b0, space3) {
	SpriteUniforms ubo;
};

struct PSInput {
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
	float2 localCoord : TEXCOORD1;
};

float4 main(PSInput input) : SV_TARGET {
	float4 texColor = main_tex.Sample(tex_sampler, input.texCoord);

	float3 opacity = ubo.opacity.xyz / 255.0;

	if (opacity.z > 0) {
		texColor *= input.localCoord.y < opacity.z ? opacity.x : opacity.y;
	} else {
		texColor *= opacity.x;
	}

	return texColor;
}
