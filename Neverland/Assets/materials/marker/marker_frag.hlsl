// Маркер кисти/блока: полупрозрачные грани (цвет и альфа — из вершин, без света и текстур).
// alphaMode=2 в материале включает блендинг (рендер-стейт движка читает это поле).
cbuffer MATERIAL_FIELDS : register(b0) {
    uint alphaMode;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float2 fragTexCoord : TEXCOORD2;
    float4 fragColor : TEXCOORD3;
    float fragLight : TEXCOORD4;
    nointerpolation uint fragId : TEXCOORD5;
};

float4 main(PSInput input) : SV_Target0 {
    // saturate(alphaMode) — формальное использование поля, чтобы рефлексия его не выкинула.
    return float4(input.fragColor.rgb, input.fragColor.a * saturate((float) alphaMode));
}
