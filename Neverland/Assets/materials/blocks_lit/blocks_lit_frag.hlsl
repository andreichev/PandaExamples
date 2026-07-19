cbuffer MATERIAL_FIELDS : register(b0) {
    float4 baseColorFactor; // @color
    float alphaCutoff;
    float daylight; // 0..1 — гасит солнечный канал (SunCycle пишет каждый кадр)
};

// Вход PS зеркалит выход VS вместе с SV_POSITION: DXBC связывает стадии по регистрам.
struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float2 fragTexCoord : TEXCOORD2;
    float4 fragColor : TEXCOORD3;
    float fragLight : TEXCOORD4;
    nointerpolation uint fragId : TEXCOORD5;
    bool isFrontFace : SV_IsFrontFace;
};

Texture2D baseColorTexture;
SamplerState baseColorTextureSampler;

// Воксельный свет построек (MC-модель): fragLight несёт упакованные каналы
// floor(sun*255)*256 + block*255 (константа по грани — интерполяция безопасна),
// АО — в альфе fragColor. Итог = max(sun*daylight, block); сценический
// directional не используется — облик стабильный, ночь делает daylight.
float4 main(PSInput input) : SV_Target0 {
    float4 baseColorSample = baseColorTexture.Sample(baseColorTextureSampler, input.fragTexCoord);

    float alpha = baseColorSample.a * baseColorFactor.a;
    if (alpha < alphaCutoff) {
        discard;
    }

    float packed = input.fragLight;
    float sunChannel = floor(packed / 256.0) / 255.0;
    float blockChannel = frac(packed / 256.0) * 256.0 / 255.0;
    float ambientOcclusion = input.fragColor.a;
    float nightFloor = 0.045; // ночь не абсолютно чёрная
    float light = max(max(sunChannel * daylight, blockChannel), nightFloor) * ambientOcclusion;

    float3 baseColor = baseColorSample.rgb * baseColorFactor.rgb * input.fragColor.rgb;
    return float4(baseColor * light, 1.0);
}
