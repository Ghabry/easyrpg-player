// SPDX-License-Identifier: MIT-0

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

float3 blendHardLight(float3 base, float3 blend) {
	// Multiply (blend <= 0.5)
	float3 multiplyResult = 2.0 * base * blend;

	// Screen (blend > 0.5)
	float3 screenResult = 1.0 - 2.0 * (1.0 - base) * (1.0 - blend);

	return lerp(multiplyResult, screenResult, step(0.5, blend));
}

float4 main(PSInput input) : SV_TARGET {
	float4 texColor = main_tex.Sample(tex_sampler, input.texCoord);

	if (texColor.a > 0.0) {
		float4 tone = ubo.tone / 255.0;
		float3 neutralTone = 128.0 / 255.0;

		if (abs(tone.a - neutralTone.r) > 0.001) {
			float3 saturationResult = blendSaturation(texColor.rgb, ubo.tone.a);
			texColor.rgb = lerp(texColor.rgb, saturationResult, texColor.a);
		}

		if (any(abs(tone.rgb - neutralTone) > 0.001)) {
			float3 hardLightResult = blendHardLight(texColor.rgb, tone.rgb);
			texColor.rgb = lerp(texColor.rgb, hardLightResult, texColor.a);
		}

		// Apply Flash effect
		if (ubo.flash.a > 0.0) {
			float3 flashColor = ubo.flash.rgb / 255.0;
			float flashStrength = ubo.flash.a / 255.0;
			texColor.rgb = lerp(texColor.rgb, flashColor, flashStrength * texColor.a);
		}
	}

	float3 opacity = ubo.opacity.xyz / 255.0;

	if (opacity.z > 0.0) {
		texColor *= input.localCoord.y < opacity.z ? opacity.x : opacity.y;
	} else {
		texColor *= opacity.x;
	}

	return texColor;
}
