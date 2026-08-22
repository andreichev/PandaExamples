// Атмосферная перспектива (стилизованный Rayleigh): чем дальше поверхность, тем сильнее
// её цвет гаснет и замещается цветом атмосферы — дальнее темнеет, уходит в голубой и
// теряет контраст (сжатие к одному цвету). Это НЕ туман: без высотной пелены, эффект
// строго по дистанции. Пасс стоит ПЕРВЫМ в цепочке — работает в линейном HDR до тонмапа.
//
// Рассеяние пер-канальное (β ∝ 1/λ⁴): синий канал сцены гаснет первым, и синяя добавка
// атмосферы растёт быстрее — горизонт синеет, как в жизни.
cbuffer MATERIAL_FIELDS : register(b0) {
    float4 atmosphereColor; // @color
    float density; // плотность: 0.0012 ≈ половинное рассеяние синего на ~600 м
};

cbuffer POST_DATA : register(b1) {
    float4 resolutionTime;
    float4 cameraNearFar; // x — near, y — far
    float4 sunScreen;
};

Texture2D inputTexture;
SamplerState inputTextureSampler;
Texture2D depthTexture;
SamplerState depthTextureSampler;

struct PSInput {
    float4 position : SV_POSITION;
    float2 fragTexCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0 {
    float4 scene = inputTexture.Sample(inputTextureSampler, input.fragTexCoord);
    float depth = depthTexture.Sample(depthTextureSampler, input.fragTexCoord).r;
    // Небо не трогаем: у него своя атмосфера (sky_gradient), двойное окрашивание
    // сдвинуло бы и солнце с bloom.
    [branch]
    if (depth >= 0.99999) { return scene; }

    // Линеаризация нелинейного depth в view-дистанцию (ZO-формула; для дымки на
    // километрах точная конвенция клипа некритична — важна монотонность, кривизну
    // компенсирует density).
    const float near = cameraNearFar.x;
    const float far = cameraNearFar.y;
    const float viewDistance = near * far / max(far - depth * (far - near), 1.e-4);

    // Rayleigh: оптическая толщина пер-канально, β нормирован к синему.
    const float3 beta = float3(0.30, 0.55, 1.0);
    const float3 transmittance = exp(-beta * (viewDistance * density));
    const float3 color =
        scene.rgb * transmittance + atmosphereColor.rgb * (1.0 - transmittance);
    return float4(color, scene.a);
}
