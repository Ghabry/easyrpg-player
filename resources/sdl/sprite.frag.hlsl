// SPDX-License-Identifier: MIT-0

// Sprite to draw on
Texture2D<float4> tex_main : register(t0, space2);
SamplerState sampler_main : register(s0, space2);

// Background layer (usually the screen) to blend with
// Only used when the BlendMode is 3 (XOR) or higher
// In all other cases the content is undefined! Do not use it!
Texture2D<float4> tex_bg : register(t1, space2);
SamplerState sampler_bg : register(s1, space2);

// Blend Modes: Must match Bitmap::BlendMode
// Modes < 3 are just alpha vs. no alpha blend. These are handled through opacity.a
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

float3 blendSaturation(float3 base, float tone_gray) {
	// Y' = 0.299 R' + 0.587 G' + 0.114 B'
	float lum = dot(base, float3(0.299, 0.587, 0.114));

	float sat = tone_gray > 128.0
		? (1024.0 + (tone_gray - 128.0) * 16.0) / 1024.0
		: (tone_gray * 8.0) / 1024.0;

	// Scale Cb/Cr by scale factor "sat"
	float3 result = lum + (base - lum) * sat;

	return saturate(result);
}

float3 blendOverlay(float3 base, float3 blend) {
	// Multiply (blend <= 0.5)
	float3 multiplyResult = 2.0 * base * blend;
	// Screen (blend > 0.5)
	float3 screenResult = 1.0 - 2.0 * (1.0 - base) * (1.0 - blend);
	return lerp(multiplyResult, screenResult, step(0.5, blend));
}

float3 blendColorBurn(float3 src, float3 dst) {
	return dst >= 1.0 ? 1.0 :
		src <= 0.0 ? 0.0 :
			1.0 - saturate((1.0 - dst) / max(src, 0.0001));
}

float3 blendSoftLight(float3 src, float3 dst) {
	float3 d = step(0.25, dst) ? sqrt(dst) :
		((16.0 * dst - 12.0) * dst + 4.0) * dst;
	float3 r1 = dst - (1.0 - 2.0 * src) * dst * (1.0 - dst);
	float3 r2 = dst + (2.0 * src - 1.0) * (d - dst);
	return lerp(r1, r2, step(0.5, src));
}

float2 applyWaver(float2 texCoord, float2 localCoord, float4 uv_rect, float depth, float phase) {
	uint tex_w, tex_h;
	tex_main.GetDimensions(tex_w, tex_h);

	float height = uv_rect.w * tex_h;

	float sy = (localCoord.y * height) * (6.28318530718 / 32.0);
	float offset = round(-2.0 * depth * sin(phase + sy));

	float2 wave_uv = texCoord;
	wave_uv.x += offset / (float)tex_w;

	return wave_uv;
}

float4 main(PSInput input) : SV_TARGET {
	float2 sampleCoord = input.texCoord;

	if (ubo.waver_depth > 0.0) {
		sampleCoord = applyWaver(input.texCoord, input.localCoord, input.uv_rect, ubo.waver_depth, ubo.waver_phase);
		if (sampleCoord.x < input.uv_rect.x || sampleCoord.x > input.uv_rect.x + input.uv_rect.z ||
			sampleCoord.y < input.uv_rect.y || sampleCoord.y > input.uv_rect.y + input.uv_rect.w) {
			// Prevent that other pixels of the spritesheet "leak"
			return float4(0.0, 0.0, 0.0, 0.0);
		}
	}

	float4 texColor = tex_main.Sample(sampler_main, sampleCoord);

	// Image is opaque
	if (ubo.opacity.a == 0.0) {
		texColor.a = 1.0;
	}

	if (texColor.a > 0.0) {
		// Effects only applied to non-transparent pixels
		float4 tone = ubo.tone / 255.0;
		float3 neutralTone = 128.0 / 255.0;

		// Apply Saturation
		if (abs(tone.a - neutralTone.r) > 0.001) {
			float3 saturationResult = blendSaturation(texColor.rgb, ubo.tone.a);
			texColor.rgb = lerp(texColor.rgb, saturationResult, texColor.a);
		}

		// Apply Tone
		if (any(abs(tone.rgb - neutralTone) > 0.001)) {
			float3 hardLightResult = blendOverlay(texColor.rgb, tone.rgb);
			texColor.rgb = lerp(texColor.rgb, hardLightResult, texColor.a);
		}

		// Apply Flash effect
		if (ubo.flash.a > 0.0) {
			float3 flashColor = ubo.flash.rgb / 255.0;
			float flashStrength = ubo.flash.a / 255.0;
			texColor.rgb = lerp(texColor.rgb, flashColor, flashStrength * texColor.a);
		}
	}

	// Opacity
	float3 opacity = ubo.opacity.xyz / 255.0;

	if (opacity.z > 0.0) {
		texColor *= input.localCoord.y < opacity.z ? opacity.x : opacity.y;
	} else {
		texColor *= opacity.x;
	}

	float4 finalColor;

	// All these fancy post-processing blend modes
	if (ubo.blend_mode >= BlendMode_XOR && ubo.blend_mode <= BlendMode_HardLight) {
		int3 bgCoord = int3(input.position.xy, 0);
		float4 dstColor = tex_bg.Load(bgCoord);

		if (ubo.blend_mode == BlendMode_Additive) {
			finalColor.rgb = saturate(texColor.rgb + dstColor.rgb);
			finalColor.a = saturate(texColor.a + dstColor.a);
		} else if (ubo.blend_mode == BlendMode_Multiply) {
			finalColor.rgb = texColor.rgb * dstColor.rgb + texColor.rgb * (1.0 - dstColor.a) + dstColor.rgb * (1.0 - texColor.a);
			finalColor.a = texColor.a + dstColor.a - texColor.a * dstColor.a;
		} else {
			float3 srcRGB = texColor.a > 0.0 ? texColor.rgb / texColor.a : 0.0;
			float3 dstRGB = dstColor.a > 0.0 ? dstColor.rgb / dstColor.a : 0.0;

			float3 blendedRGB = srcRGB;
			float srcAlphaFactor = texColor.a;

			if (ubo.blend_mode == BlendMode_Overlay) {
				blendedRGB = blendOverlay(srcRGB, dstRGB);
			} else if (ubo.blend_mode == BlendMode_Screen) {
				blendedRGB = srcRGB + dstRGB - srcRGB * dstRGB;
			} else if (ubo.blend_mode == BlendMode_Saturate) {
				blendedRGB = srcRGB;
				srcAlphaFactor = min(texColor.a, 1.0 - dstColor.a);
			} else if (ubo.blend_mode == BlendMode_Darken) {
				blendedRGB = min(srcRGB, dstRGB);
			} else if (ubo.blend_mode == BlendMode_Lighten) {
				blendedRGB = max(srcRGB, dstRGB);
			} else if (ubo.blend_mode == BlendMode_ColorDodge) {
				// Is the inverted version of ColorBurn
				blendedRGB = 1.0 - blendColorBurn(1.0 - srcRGB, 1.0 - dstRGB);
			} else if (ubo.blend_mode == BlendMode_ColorBurn) {
				blendedRGB = blendColorBurn(srcRGB, dstRGB);
			} else if (ubo.blend_mode == BlendMode_Difference) {
				blendedRGB = abs(dstRGB - srcRGB);
			} else if (ubo.blend_mode == BlendMode_Exclusion) {
				blendedRGB = dstRGB + srcRGB - 2.0 * dstRGB * srcRGB;
			} else if (ubo.blend_mode == BlendMode_SoftLight) {
				blendedRGB = blendSoftLight(srcRGB, dstRGB);
			} else if (ubo.blend_mode == BlendMode_HardLight) {
				// HardLight is Overlay with swapped inputs
				blendedRGB = blendOverlay(dstRGB, srcRGB);
			}

			// Re-composite with premultiplied alpha
			finalColor.rgb = srcAlphaFactor * dstColor.a * blendedRGB
						   + texColor.rgb * (1.0 - dstColor.a)
						   + dstColor.rgb * (1.0 - srcAlphaFactor);
			finalColor.a = srcAlphaFactor + dstColor.a * (1.0 - srcAlphaFactor);
		}
	} else {
		// No postprocessing blend mode
		finalColor = texColor;
	}

	return finalColor;
}
