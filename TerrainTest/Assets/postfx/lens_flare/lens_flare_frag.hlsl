// Lens flare (адаптация shadertoy-версии юзера): гоусты-круги/кольца/гексагоны вдоль
// оси солнца, лучи и свечение вокруг него. Пасс display-пространства: стоит в цепочке
// ПОСЛЕ тонмапа, флейр аддитивен к готовому кадру (фоновое небо оригинала выброшено).
// Позиция солнца — POST_DATA.sunScreen (движок проецирует DirectionalLight на экран);
// окклюзия — семплы emissiveTexture вокруг точки солнца: листва перекрыла диск →
// emissive в точке гаснет → флейр гаснет, без depth-теста.
cbuffer MATERIAL_FIELDS : register(b0) {
    float flareStrength; // общая сила флейра (0 — выкл)
};

cbuffer POST_DATA : register(b1) {
    float4 resolutionTime; // xy — размер таргета px
    float4 cameraNearFar;
    float4 sunScreen;      // xy — UV солнца, z — «в кадре» (0/1)
};

Texture2D inputTexture;
SamplerState inputTextureSampler;
Texture2D emissiveTexture;
SamplerState emissiveTextureSampler;

struct PSInput {
    float4 position : SV_POSITION;
    float2 fragTexCoord : TEXCOORD0;
};

float rnd(float w) {
    return frac(sin(w) * 1000.0);
}

// Правильный многоугольник (гексагоны диафрагмы).
float regShape(float2 p, int N) {
    float a = atan2(p.x, p.y) + 0.2;
    float b = 6.28319 / float(N);
    return smoothstep(0.5, 0.51, cos(floor(0.5 + a / b) * b - a) * length(p.xy));
}

// Один элемент флейра: большой круг + кольцо + малый круг + гексагон (порт circle()).
float3 circleGhost(float2 p, float size, float dist, float2 sunPos, float index) {
    float l = length(p + sunPos * (dist * 4.0)) + size / 2.0;
    float c = max(0.01 - pow(length(p + sunPos * dist), size * 1.4), 0.0) * 50.0;
    float c1 = max(0.001 - pow(l - 0.3, 1.0 / 40.0) + sin(l * 30.0), 0.0) * 3.0;
    // У оригинала здесь минус — единственный член НА СТОРОНЕ СОЛНЦА (+sunPos·dist/2):
    // при dist ≈ 2 кружок ложился почти на солнце, а при dist > 2 двигался БЫСТРЕЕ
    // солнца в ту же сторону — читался «вторым солнцем, обгоняющим настоящее».
    // Знак + ставит его на общую анти-солнечную ось со всеми остальными членами.
    float c2 = max(0.04 / pow(length(p + sunPos * dist / 2.0 + 0.09) * 1.0, 1.0), 0.0) / 20.0;
    float s = max(0.01 - pow(regShape(p * 5.0 + sunPos * dist * 5.0 + 0.9, 6), 1.0), 0.0) * 5.0;
    float3 color = cos(float3(0.44, 0.24, 0.2) * 8.0 + dist * 4.0) * 0.5 + 0.5;
    return (c + c1 + c2 + s) * color - 0.01;
}

float4 main(PSInput input) : SV_Target0 {
    float4 scene = inputTexture.Sample(inputTextureSampler, input.fragTexCoord);
    [branch]
    if (sunScreen.z < 0.5 || flareStrength <= 0.0) { return scene; }

    // Окклюзия: средний emissive в пяти точках вокруг солнца; /10 нормирует HDR-яркость
    // диска (~60) так, что частичное перекрытие кроной плавно гасит флейр.
    float visibility = 0.0;
    const float2 occlusionOffsets[5] = {
        float2(0.0, 0.0), float2(0.008, 0.0), float2(-0.008, 0.0), float2(0.0, 0.008), float2(0.0, -0.008)
    };
    [unroll]
    for (int i = 0; i < 5; i++) {
        float3 e = emissiveTexture.Sample(emissiveTextureSampler, sunScreen.xy + occlusionOffsets[i]).rgb;
        visibility += dot(e, float3(0.2126, 0.7152, 0.0722));
    }
    visibility = saturate(visibility / 5.0 / 10.0);
    [branch]
    if (visibility <= 0.001) { return scene; }

    const float aspect = resolutionTime.x / max(resolutionTime.y, 1.0);
    float2 uv = input.fragTexCoord - 0.5;
    uv.x *= aspect;
    float2 sunPos = sunScreen.xy - 0.5;
    sunPos.x *= aspect;

    // Диска/свечения «своего солнца» в эффекте нет — солнце рисует небо (sky_gradient),
    // дубль давал второй диск. Состав: гоусты вдоль оси + лучи вокруг реального солнца.
    float3 flare = float3(0.0, 0.0, 0.0);

    // Гоусты (10 элементов, дистанции оригинала). Фейда от центра НЕТ: гоусты живут при
    // любом положении солнца в кадре; гасит их только окклюзия листвой (visibility).
    float3 ghosts = float3(0.0, 0.0, 0.0);
    [loop]
    for (int g = 0; g < 10; g++) {
        float fi = float(g);
        ghosts += circleGhost(
            uv, pow(rnd(fi * 2000.0) * 1.8, 2.0) + 1.41, rnd(fi * 20.0) * 3.0 + 0.2 - 0.5, sunPos, fi
        );
    }
    flare += max(ghosts, 0.0) * 0.6;

    // Лучи из оригинала, БЕЗ его диска и «пелены»: тонкая корона у солнца + длинные
    // лучи с собственным радиальным спадом (вместо глобальной exp-маски, которая
    // требовала солнце в центре). Привязаны к реальному солнцу — видны при любом его
    // положении в кадре.
    float2 toSun = uv - sunPos;
    float a = atan2(toSun.y, toSun.x);
    float d = length(toSun);
    // Лучи центрированы на РЕАЛЬНОМ солнце и живут всегда, пока оно в кадре (гасит их
    // только окклюзия листвой через visibility) — никаких фейдов от центра кадра.
    float rays = max(0.1 / pow(d * 5.0, 5.0), 0.0) * abs(sin(a * 5.0 + cos(a * 9.0))) / 20.0;
    rays += abs(sin(a * 3.0 + cos(a * 9.0))) / 8.0 * abs(sin(a * 9.0)) * exp(-d * 2.2);

    // Окклюзия: гоусты гаснут пропорционально перекрытию, лучи — втрое слабее (bloom
    // просвечивает сквозь крону — пусть его сопровождают и лучи; гаснут только при
    // почти полном перекрытии солнца).
    float raysVisibility = saturate(visibility * 3.0);
    float3 result = scene.rgb + max(flare, 0.0) * visibility * flareStrength +
                    max(rays, 0.0) * float3(1.0, 0.95, 0.85) * raysVisibility * flareStrength;
    return float4(result, scene.a);
}
