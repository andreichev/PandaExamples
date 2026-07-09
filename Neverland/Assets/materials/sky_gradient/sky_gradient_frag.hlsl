cbuffer MATERIAL_FIELDS : register(b0) {
    float4 dayTopColor;      // @color
    float4 dayHorizonColor;  // @color
    float4 duskColor;        // @color
    float4 nightTopColor;    // @color
    float4 nightHorizonColor; // @color
    float cycleSeconds;
    float nightBrightness;
};

cbuffer PANDA_FIELDS : register(b1) {
    float time;
};

// Вход PS зеркалит выход VS вместе с SV_POSITION: DXBC связывает стадии по регистрам.
struct PSInput {
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0 {
    float3 direction = normalize(input.direction);
    float y = saturate(direction.y * 0.5 + 0.5);
    float horizon = pow(y, 0.65);

    float phase = time / max(cycleSeconds, 1.0) * 6.2831853;
    float sunAmount = saturate(sin(phase) * 0.5 + 0.5);
    float dayAmount = smoothstep(0.12, 0.70, sunAmount);
    float duskAmount = pow(1.0 - abs(sunAmount * 2.0 - 1.0), 3.0);

    float3 dayColor = lerp(dayHorizonColor.rgb, dayTopColor.rgb, horizon);
    float3 nightColor = lerp(nightHorizonColor.rgb, nightTopColor.rgb, pow(y, 0.8));
    nightColor *= max(nightBrightness, 0.0);

    float3 color = lerp(nightColor, dayColor, dayAmount);
    color = lerp(color, duskColor.rgb, duskAmount * 0.32);

    float3 sunDirection = normalize(float3(cos(phase), sin(phase), -0.22));
    float sunDisk = smoothstep(0.9990, 0.9998, dot(direction, sunDirection)) * dayAmount;
    color += sunDisk * float3(1.0, 0.86, 0.52);

    return float4(color, 1.0);
}
