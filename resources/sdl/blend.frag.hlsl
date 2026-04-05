// SPDX-License-Identifier: MIT-0

// Sprite we draw on
Texture2D<float4> tex_src : register(t0, space2);
SamplerState sampler_src : register(s0, space2);

// Background layer we blend with
Texture2D<float4> tex_bg : register(t1, space2);
SamplerState sampler_bg : register(s1, space2);

// Must match Bitmap::BlendMode
static const int BlendMode_Default = 0;
static const int BlendMode_Normal = 1;
static const int BlendMode_NormalWithoutAlpha = 2;
static const int BlendMode_XOR = 3;
static const int BlendMode_Additive = 4;
static const int BlendMode_Multiply = 5;
static const int BlendMode_Overlay = 6;
static const int BlendMode_Screen = 7;
static const int BlendMode_Saturate = 8;
static const int BlendMode_Darken = 9;
static const int BlendMode_Lighten = 10;
static const int BlendMode_Substract = 11;
static const int BlendMode_ColorDodge = 12;
static const int BlendMode_ColorBurn = 13;
static const int BlendMode_Difference = 14;
static const int BlendMode_Exclusion = 15;
static const int BlendMode_SoftLight = 16;
static const int BlendMode_HardLight = 17;

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
	float4 uv_rect : TEXCOORD2;
};

float3 overlayBlend(float3 src, float3 dst) {
	// Multiply (dst <= 0.5)
	float3 multiplyResult = 2.0 * src * dst;

	// Screen (blend > 0.5)
	float3 screenResult = 1.0 - 2.0 * (1.0 - src) * (1.0 - dst);

	return lerp(multiplyResult, screenResult, step(0.5, dst));
}

float4 main(PSInput input) : SV_TARGET
{
	float4 srcColor = tex_src.Sample(sampler_src, input.texCoord);

	int3 bgCoord = int3(input.position.xy, 0);
	float4 dstColor = tex_bg.Load(bgCoord);

	if (ubo.blend_mode == BlendMode_Additive || ubo.blend_mode == BlendMode_Multiply || ubo.blend_mode == BlendMode_Overlay) {
		// Un-premultiply colors
		float3 srcRGB = srcColor.a > 0.0 ? srcColor.rgb / srcColor.a : 0.0;
		float3 dstRGB = dstColor.a > 0.0 ? dstColor.rgb / dstColor.a : 0.0;

		float3 blendedRGB;
		if (ubo.blend_mode == BlendMode_Additive) {
			blendedRGB = min(srcRGB + dstRGB, 1.0);
		} else if (ubo.blend_mode == BlendMode_Multiply) {
			blendedRGB = srcRGB * dstRGB;
		} else if (ubo.blend_mode == BlendMode_Overlay) {
			blendedRGB = overlayBlend(srcRGB, dstRGB);
		}

		// Re-composite with premultiplied alpha
		finalColor.rgb = srcColor.a * dstColor.a * blendedRGB
					   + srcColor.rgb * (1.0 - dstColor.a)
					   + dstColor.rgb * (1.0 - srcColor.a);
		finalColor.a = srcColor.a + dstColor.a * (1.0 - srcColor.a);
	} else {
		// Default (unsupported blend mode)
		finalColor.rgb = srcColor.rgb + dstColor.rgb * (1.0f - srcColor.a);
		finalColor.a = srcColor.a + dstColor.a * (1.0f - srcColor.a);
	}

	return finalColor;
}
