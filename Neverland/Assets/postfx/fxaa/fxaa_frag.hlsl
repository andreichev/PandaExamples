// FXAA (console-вариант 3.11): сглаживание геометрических кромок без MSAA.
// Работает в линейном HDR до неявного тонемапа; люма оценивается по сжатому
// цвету (c / (1 + c)), сами цвета смешиваются в HDR.
cbuffer POST_DATA {
    float4 resolutionTime;
    float4 cameraNearFar;
};

Texture2D inputTexture;
SamplerState inputTextureSampler;

struct PSInput {
    float4 position : SV_POSITION;
    float2 fragTexCoord : TEXCOORD0;
};

float3 sampleColor(float2 uv) {
    return inputTexture.Sample(inputTextureSampler, uv).rgb;
}

float lumaOf(float3 color) {
    float3 mapped = color / (1.0 + color);
    return dot(mapped, float3(0.299, 0.587, 0.114));
}

float4 main(PSInput input) : SV_Target0 {
    float2 rcpResolution = 1.0 / resolutionTime.xy;
    float2 uv = input.fragTexCoord;

    float3 rgbM = sampleColor(uv);
    float lumaM = lumaOf(rgbM);
    float lumaNW = lumaOf(sampleColor(uv + float2(-0.5, -0.5) * rcpResolution));
    float lumaNE = lumaOf(sampleColor(uv + float2(0.5, -0.5) * rcpResolution));
    float lumaSW = lumaOf(sampleColor(uv + float2(-0.5, 0.5) * rcpResolution));
    float lumaSE = lumaOf(sampleColor(uv + float2(0.5, 0.5) * rcpResolution));

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    if (lumaMax - lumaMin < max(0.0312, lumaMax * 0.125)) {
        return float4(rgbM, 1.0); // контраста нет — кромки нет
    }

    float2 dir = float2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)), (lumaNW + lumaSW) - (lumaNE + lumaSE));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.125, 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, float2(-8.0, -8.0), float2(8.0, 8.0)) * rcpResolution;

    float3 rgbA = 0.5 * (sampleColor(uv + dir * (1.0 / 3.0 - 0.5)) + sampleColor(uv + dir * (2.0 / 3.0 - 0.5)));
    float3 rgbB = rgbA * 0.5 + 0.25 * (sampleColor(uv + dir * -0.5) + sampleColor(uv + dir * 0.5));
    float lumaB = lumaOf(rgbB);
    if (lumaB < lumaMin || lumaB > lumaMax) {
        return float4(rgbA, 1.0);
    }
    return float4(rgbB, 1.0);
}
