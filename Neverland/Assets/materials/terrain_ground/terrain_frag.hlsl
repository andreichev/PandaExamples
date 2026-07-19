// Terrain: блендинг четырёх материалов ground-атласа (6×6) по вершинным весам.
// fragColor = веса (r=трава, g=земля, b=камень, a=песок); fragTexCoord — НОРМАЛИЗОВАННЫЙ
// UV внутри тайла (0..1, mirror-повтор пишет генератор). Плавные границы между материалами
// получаются интерполяцией весов по треугольнику.

cbuffer MATERIAL_FIELDS : register(b0) {
    float4 baseColorFactor; // @color
    float alphaCutoff;
    // Светокарта мира (атлас Y-слоёв): (minX, minZ, размер участка в ячейках, тайлов в ряду).
    float4 lightMapParams;
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
// Воксельный свет (LightGrid игры): R — солнце 0..1, G — источники 0..1.
Texture2D lightMap;
SamplerState lightMapSampler;

// Семпл света по мировой позиции: слой атласа по Y, билинейный по XZ, lerp слоёв.
float2 sampleVoxelLight(float3 worldPos) {
    float cells = max(lightMapParams.z, 1.0);
    float tilesPerRow = max(lightMapParams.w, 1.0);
    float atlasSize = cells * tilesPerRow;
    float layerCount = tilesPerRow * tilesPerRow;

    float2 local = float2(worldPos.x - lightMapParams.x, worldPos.z - lightMapParams.y);
    // Полутексельный инсет — билинейка не тянет соседний слой атласа.
    local = clamp(local, 0.5, cells - 0.5);

    float layerF = clamp(worldPos.y, 0.0, layerCount - 1.0);
    float layer0 = floor(layerF);
    float layer1 = min(layer0 + 1.0, layerCount - 1.0);

    float2 tile0 = float2(fmod(layer0, tilesPerRow), floor(layer0 / tilesPerRow));
    float2 tile1 = float2(fmod(layer1, tilesPerRow), floor(layer1 / tilesPerRow));
    float2 light0 = lightMap.Sample(lightMapSampler, (tile0 * cells + local) / atlasSize).rg;
    float2 light1 = lightMap.Sample(lightMapSampler, (tile1 * cells + local) / atlasSize).rg;
    return lerp(light0, light1, frac(layerF));
}

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

    // Воксельный свет: солнечный канал затеняет сценическое освещение (тень под
    // крышей/деревом), канал источников добавляет тёплый свет ламп (работает ночью).
    float2 voxelLight = sampleVoxelLight(input.worldPos);
    float shadowFactor = lerp(0.32, 1.0, voxelLight.r);
    float3 lampColor = float3(1.0, 0.86, 0.62) * voxelLight.g * 0.85;

    float3 litColor = baseColor * (lighting * shadowFactor + lampColor) * input.fragLight;
    return float4(litColor, 1.0);
}
