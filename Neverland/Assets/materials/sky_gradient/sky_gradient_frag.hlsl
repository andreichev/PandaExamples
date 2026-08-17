// ВАЖНО: поля материала биндятся ПО ПОРЯДКУ — новые поля только В КОНЕЦ (контракт MATERIAL_FIELDS).
cbuffer MATERIAL_FIELDS : register(b0) {
    float4 dayTopColor;      // @color
    float4 dayHorizonColor;  // @color
    float4 duskColor;        // @color
    float4 nightTopColor;    // @color
    float4 nightHorizonColor; // @color
    float nightBrightness;
    float4 sunDirectionDay;  // xyz — направление на солнце, w — dayAmount (0..1); пишет SunCycle
    float duskAmount;
    // Солнечный диск (контракт: новые поля только В КОНЕЦ). Размеры — угловые градусы;
    // диск крупнее реального солнца (~0.5°) — так читается лучше. Пишется и в emissive
    // (MRT) — bloom спускается с него без порога и даёт сияние.
    float sunAngularSize;
    float sunHalo;
    float sunIntensity;
};

cbuffer PANDA_FIELDS : register(b1) {
    float time;
};

// Вход PS зеркалит выход VS вместе с SV_POSITION: DXBC связывает стадии по регистрам.
struct PSInput {
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
};

// MRT сцены: второй выход — emissive-буфер (его читает bloom; пишем туда солнце).
struct PSOutput {
    float4 color : SV_Target0;
    float4 emissive : SV_Target1;
};

PSOutput main(PSInput input) {
    float3 direction = normalize(input.direction);
    float y = saturate(direction.y * 0.5 + 0.5);
    float horizon = pow(y, 0.65);

    float dayAmount = saturate(sunDirectionDay.w);

    float3 dayColor = lerp(dayHorizonColor.rgb, dayTopColor.rgb, horizon);
    float3 nightColor = lerp(nightHorizonColor.rgb, nightTopColor.rgb, pow(y, 0.8));
    nightColor *= max(nightBrightness, 0.0);

    float3 color = lerp(nightColor, dayColor, dayAmount);
    color = lerp(color, duskColor.rgb, saturate(duskAmount) * 0.32);

    // Солнце из трёх компонент, углом от центра (acos), а не порогом косинуса:
    //  ядро  — слепящий диск с короткой кромкой;
    //  мгла  — экспоненциальный спад от кромки (~1.2 радиуса диска): контур растворяется,
    //          как у реального солнца, вместо «круга с обводкой»;
    //  гало  — широкое слабое сияние до sunHalo.
    // HDR-яркость ядра высокая (sunIntensity ~30): bloom строится ДАУНСЕМПЛОМ emissive,
    // и частично перекрытое листвой солнце иначе усредняется в ничто — сияние сквозь
    // кроны живёт именно на запасе яркости.
    float3 sun = float3(0.0, 0.0, 0.0);
    [branch]
    if (sunIntensity > 0.0 && dayAmount > 0.0) {
        float3 sunDirection = normalize(sunDirectionDay.xyz);
        float cosView = dot(direction, sunDirection);
        float ang = acos(clamp(cosView, -1.0, 1.0));
        float discR = radians(sunAngularSize * 0.5);
        float disc = 1.0 - smoothstep(discR * 0.8, discR, ang);
        float glow = exp(-max(ang - discR, 0.0) / max(discR * 1.2, 1e-4));
        float haloR = radians(max(sunHalo, sunAngularSize) * 0.5);
        float halo = pow(saturate(1.0 - ang / haloR), 3.0);
        sun = float3(1.0, 0.86, 0.52) * sunIntensity *
              (disc + glow * 0.25 + halo * 0.02) * dayAmount;
        color += sun;
    }

    PSOutput output;
    output.color = float4(color, 1.0);
    output.emissive = float4(sun, 1.0);
    return output;
}
