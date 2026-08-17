// Тонмап цепочки (handlesTonemap: движковый фолбэк-Reinhard не исполняется).
// ACES-аппроксимация Narkowicz + sRGB-гамма: линейный HDR → display. Пассы цепочки
// ПОСЛЕ этого работают в display-пространстве (lens flare, glare, afterimage).
cbuffer POST_DATA {
    float4 resolutionTime; // xy — размер таргета px, z — время (не используется)
    float4 cameraNearFar;
    float4 sunScreen;
};

Texture2D inputTexture;
SamplerState inputTextureSampler;

struct PSInput {
    float4 position : SV_POSITION;
    float2 fragTexCoord : TEXCOORD0;
};

float3 acesToneMap(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(PSInput input) : SV_Target0 {
    float4 source = inputTexture.Sample(inputTextureSampler, input.fragTexCoord);
    float3 color = acesToneMap(source.rgb);
    color = pow(abs(color), 1.0 / 2.2);
    return float4(color, source.a);
}
