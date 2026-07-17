// Terrain: блендинг четырёх материалов ground-атласа (6×6) по вершинным весам.
// fragColor = веса (r=трава, g=земля, b=камень, a=песок); fragTexCoord — НОРМАЛИЗОВАННЫЙ
// UV внутри тайла (0..1, mirror-повтор пишет генератор). Плавные границы между материалами
// получаются интерполяцией весов по треугольнику.

cbuffer MATERIAL_FIELDS : register(b0) {
    float4 baseColorFactor; // @color
    float alphaCutoff;
};

cbuffer PANDA_FIELDS : register(b1) {
    float time;
};

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

cbuffer SCENE_LIGHTS {
    float4 ambientColorIntensity;
    float4 directionalDirectionIntensity;
    float4 directionalColor;
    uint4 sceneLightCounts;
    float4 pointLightPositionsRange[8];
    float4 pointLightColorsIntensity[8];
};

Texture2D baseColorTexture;
SamplerState baseColorTextureSampler;

static const float TILE = 1.0 / 6.0;
static const float TILE_INSET = 0.004;
// Тайлы ground-атласа: трава (0,0), земля (4,0), камень (0,3), песок (0,2).
static const float2 TILE_BASE[4] = {
    float2(0.0, 0.0),
    float2(4.0 * TILE, 0.0),
    float2(0.0, 3.0 * TILE),
    float2(0.0, 2.0 * TILE),
};

float3 sampleLayer(int layer, float2 tileUv) {
    float2 uv = TILE_BASE[layer] + float2(TILE_INSET, TILE_INSET) + tileUv * (TILE - TILE_INSET * 2.0);
    return baseColorTexture.Sample(baseColorTextureSampler, uv).rgb;
}

float computeSpecular(float3 normal, float3 lightDirection, float roughness, float metallic) {
    float3 viewDirection = normalize(float3(0.0, 0.0, 1.0));
    float3 halfVector = normalize(lightDirection + viewDirection);
    float specularPower = lerp(64.0, 4.0, roughness);
    return pow(saturate(dot(normal, halfVector)), specularPower) * lerp(0.04, 1.0, metallic);
}

float4 main(PSInput input) : SV_Target0 {
    float4 weights = input.fragColor;
    float weightSum = weights.r + weights.g + weights.b + weights.a;
    if (weightSum <= 0.0001) {
        weights = float4(1.0, 0.0, 0.0, 0.0); // дефолт — трава
        weightSum = 1.0;
    }
    weights /= weightSum;

    float2 tileUv = saturate(input.fragTexCoord);
    float3 blended = sampleLayer(0, tileUv) * weights.r + sampleLayer(1, tileUv) * weights.g +
                     sampleLayer(2, tileUv) * weights.b + sampleLayer(3, tileUv) * weights.a;

    float3 surfaceNormal = normalize(input.worldNormal);
    float3 baseColor = blended * baseColorFactor.rgb;

    float3 lighting = ambientColorIntensity.rgb * ambientColorIntensity.w;
    if (directionalDirectionIntensity.w > 0.0) {
        float3 lightDirection = normalize(directionalDirectionIntensity.xyz);
        float3 radiance = directionalColor.rgb * directionalDirectionIntensity.w;
        float diffuse = saturate(dot(surfaceNormal, lightDirection));
        float specular = computeSpecular(surfaceNormal, lightDirection, 0.9, 0.0);
        lighting += radiance * diffuse;
        lighting += radiance * specular;
    }

    float3 litColor = baseColor * lighting * input.fragLight;
    return float4(litColor, 1.0);
}
