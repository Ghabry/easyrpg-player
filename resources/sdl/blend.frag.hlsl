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
// The shader is only called for values starting from here
// The results were visually compared with Pixman and match
static const int BlendMode_XOR = 3; // Not implemented (could not get this to match with pixman)
static const int BlendMode_Additive = 4;
static const int BlendMode_Multiply = 5;
static const int BlendMode_Overlay = 6;
static const int BlendMode_Screen = 7;
static const int BlendMode_Saturate = 8;
static const int BlendMode_Darken = 9;
static const int BlendMode_Lighten = 10;
static const int BlendMode_ColorDodge = 11;
static const int BlendMode_ColorBurn = 12;
static const int BlendMode_Difference = 13;
static const int BlendMode_Exclusion = 14;
static const int BlendMode_SoftLight = 15;
static const int BlendMode_HardLight = 16;

struct SpriteUniforms {
	float4 tone;
	float4 flash;
	float4 opacity; // top, bottom, split, image has alpha
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

float3 colorDodge(float3 src, float3 dst) {
	return dst <= 0.0 ? 0.0 : src >= 1.0 ? 1.0 : saturate(dst / max(1.0 - src, 0.0001));
}

float3 colorBurn(float3 src, float3 dst) {
	return dst >= 1.0 ? 1.0 : src <= 0.0 ? 0.0 : 1.0 - saturate((1.0 - dst) / max(src, 0.0001));
}

float3 softLight(float3 src, float3 dst) {
	float3 d = step(0.25, dst) ? sqrt(dst) : ((16.0 * dst - 12.0) * dst + 4.0) * dst;
	float3 r1 = dst - (1.0 - 2.0 * src) * dst * (1.0 - dst);
	float3 r2 = dst + (2.0 * src - 1.0) * (d - dst);
	return lerp(r1, r2, step(0.5, src));
}

float2 applyWaver(float2 texCoord, float2 localCoord, float4 uv_rect, float depth, float phase) {
	uint tex_w, tex_h;
	tex_src.GetDimensions(tex_w, tex_h);

	float height = uv_rect.w * tex_h;

	float sy = (localCoord.y * height) * (6.28318530718 / 32.0);
	float offset = round(-2.0 * depth * sin(phase + sy));

	float2 wave_uv = texCoord;
	wave_uv.x += offset / (float)tex_w;

	return wave_uv;
}

float4 main(PSInput input) : SV_TARGET
{
	float2 sampleCoord = input.texCoord;

	if (ubo.waver_depth > 0.0) {
		sampleCoord = applyWaver(input.texCoord, input.localCoord, input.uv_rect, ubo.waver_depth, ubo.waver_phase);
	}

	float4 srcColor = tex_src.Sample(sampler_src, sampleCoord);

	int3 bgCoord = int3(input.position.xy, 0);
	float4 dstColor = tex_bg.Load(bgCoord);

	// Image is opaque
	if (ubo.opacity.a == 0.0) {
		srcColor.a = 1.0;
	}

	float4 finalColor;

	if (ubo.blend_mode >= BlendMode_Additive && ubo.blend_mode <= BlendMode_HardLight) {
		// Un-premultiply colors
		float3 srcRGB = srcColor.a > 0.0 ? srcColor.rgb / srcColor.a : 0.0;
		float3 dstRGB = dstColor.a > 0.0 ? dstColor.rgb / dstColor.a : 0.0;

		float3 blendedRGB = srcRGB;

		float srcAlphaFactor = srcColor.a;

		if (ubo.blend_mode == BlendMode_Additive) {
			blendedRGB = saturate(srcRGB + dstRGB);
		} else if (ubo.blend_mode == BlendMode_Multiply) {
			blendedRGB = srcRGB * dstRGB;
		} else if (ubo.blend_mode == BlendMode_Overlay) {
			blendedRGB = overlayBlend(srcRGB, dstRGB);
		} else if (ubo.blend_mode == BlendMode_Screen) {
			blendedRGB = srcRGB + dstRGB - srcRGB * dstRGB;
		} else if (ubo.blend_mode == BlendMode_Saturate) {
			blendedRGB = srcRGB;
			srcAlphaFactor = min(srcColor.a, 1.0 - dstColor.a);
		} else if (ubo.blend_mode == BlendMode_Darken) {
			blendedRGB = min(srcRGB, dstRGB);
		} else if (ubo.blend_mode == BlendMode_Lighten) {
			blendedRGB = max(srcRGB, dstRGB);
		} else if (ubo.blend_mode == BlendMode_ColorDodge) {
			blendedRGB = colorDodge(srcRGB, dstRGB);
		} else if (ubo.blend_mode == BlendMode_ColorBurn) {
			blendedRGB = colorBurn(srcRGB, dstRGB);
		} else if (ubo.blend_mode == BlendMode_Difference) {
			blendedRGB = abs(dstRGB - srcRGB);
		} else if (ubo.blend_mode == BlendMode_Exclusion) {
			blendedRGB = dstRGB + srcRGB - 2.0 * dstRGB * srcRGB;
		} else if (ubo.blend_mode == BlendMode_SoftLight) {
			blendedRGB = softLight(srcRGB, dstRGB);
		} else if (ubo.blend_mode == BlendMode_HardLight) {
			 // HardLight is Overlay with swapped inputs
			blendedRGB = overlayBlend(dstRGB, srcRGB);
		}

		// Re-composite with premultiplied alpha
		finalColor.rgb = srcAlphaFactor * dstColor.a * blendedRGB
					   + srcColor.rgb * (1.0 - dstColor.a)
					   + dstColor.rgb * (1.0 - srcAlphaFactor);
		finalColor.a = srcAlphaFactor + dstColor.a * (1.0 - srcAlphaFactor);
	} else {
		// Default (Normal / Fallback)
		finalColor.rgb = srcColor.rgb + dstColor.rgb * (1.0f - srcColor.a);
		finalColor.a = srcColor.a + dstColor.a * (1.0f - srcColor.a);
	}

	return finalColor;
}
