// Спільна частина Lit-шейдерів: фізично коректні поверхня й освітлення в лінійному колірному просторі

// Кількість каскадів карти тіней; те саме значення визначено в GlobalResources.h
#define SHADOW_CASCADE_COUNT 4

Texture2D BaseMap : register(t0);
Texture2DArray ShadowMap : register(t1);
Texture2D GlossMap : register(t2);
Texture2D NormalMap : register(t3);
Texture2D HeightMap : register(t4);
Texture2D OcclusionMap : register(t5);
Texture2D EmissionMap : register(t6);
Texture2D DetailMask : register(t7);
Texture2D DetailAlbedoMap : register(t8);
Texture2D DetailNormalMap : register(t9);
Texture2D EnvironmentMap : register(t10);

sampler TextureSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

struct PS_INPUT {
	float4 pos: SV_POSITION0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD0;
    float3 cameraDir : TEXCOORD2;
    float3 lightDir : TEXCOORD3;
    float3 worldPos : TEXCOORD4;
};

cbuffer constant : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 model;
    row_major float4x4 invTransModel;
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 lightViewProjection[SHADOW_CASCADE_COUNT];
    float3 cameraPos;
    float cameraPosPadding;
    float3 lightPos;
    float lightPosPadding;
    float3 lightColor;
    float lightColorPadding;
    float3 lightDir;
    float lightDirPadding;
    float4 shadowParams;
    float4 cascadeSplits;
    float4 cascadeBias;
    float4 cascadeParams;
    unsigned int time;
    float3 timePadding;
    // Сила навколишнього світла, сила відбиттів, кількість mip-рівнів неба і чи є небо
    float4 environmentParams;
    // Навколишнє світло без неба, вже в лінійному просторі
    float4 ambientColor;
};

cbuffer material : register(b1)
{
    float4 baseColor;
    float4 specularColor;
    float4 emissionColor;
    float4 tilingOffset;
    float4 detailTilingOffset;
    // Металевість, гладкість, сила нормалей і висота паралаксу
    float4 surfaceParams;
    // Сила затінення, поріг альфи, сила деталей кольору й нормалей
    float4 extraParams;
    // Наявні карти, увімкнені можливості, режим змішування
    uint4 flags;
};

// Біти наявних карт; мають збігатися з Material.cpp
#define MAP_BASE 1
#define MAP_GLOSS 2
#define MAP_NORMAL 4
#define MAP_HEIGHT 8
#define MAP_OCCLUSION 16
#define MAP_EMISSION 32
#define MAP_DETAIL_MASK 64
#define MAP_DETAIL_ALBEDO 128
#define MAP_DETAIL_NORMAL 256

// Біти можливостей матеріалу; мають збігатися з Material.cpp
#define FEATURE_SPECULAR_WORKFLOW 1
#define FEATURE_SMOOTHNESS_ALBEDO_ALPHA 2
#define FEATURE_ALPHA_CLIP 4
#define FEATURE_EMISSION 8
#define FEATURE_RECEIVE_SHADOWS 16
#define FEATURE_SPECULAR_HIGHLIGHTS 32
#define FEATURE_ENVIRONMENT_REFLECTIONS 64
#define FEATURE_TRANSPARENT 128

// Режими змішування прозорої поверхні
#define BLEND_ALPHA 0
#define BLEND_PREMULTIPLY 1
#define BLEND_ADDITIVE 2
#define BLEND_MULTIPLY 3

static const float PI = 3.14159265f;

// Наскільки далеко вздовж нормалі зсувається точка перед читанням карти тіней, у текселях каскаду
static const float NORMAL_OFFSET_TEXELS = 1.5f;
// Косинус кута до світла, нижче якого поверхня темнішає плавно за кутом, а не за картою тіней
static const float TERMINATOR_FADE = 0.25f;

// Перевіряє, чи в матеріалу є карта
bool hasMap(uint bit)
{
    return (flags.x & bit) != 0;
}

// Перевіряє, чи в матеріалу увімкнена можливість
bool hasFeature(uint bit)
{
    return (flags.y & bit) != 0;
}

// Переводить колір із sRGB у лінійний простір для кольорів і кольорових текстур
float3 srgbToLinear(float3 c)
{
    return c <= 0.04045f ? c / 12.92f : pow(abs((c + 0.055f) / 1.055f), 2.4f);
}

// Застосовує масштаб і зсув текстури так, ніби V росте вгору, бо сітка рушія перевернута по V
float2 transformUv(float2 uv, float4 scaleOffset)
{
    float2 unityUv = float2(uv.x, 1.0f - uv.y) * scaleOffset.xy + scaleOffset.zw;

    return float2(unityUv.x, 1.0f - unityUv.y);
}

// Переводить напрямок у координати небесної текстури так само, як її накладає меш неба
float2 directionToSky(float3 direction)
{
    return float2(atan2(-direction.z, -direction.x) / (2.0f * PI) + 0.5f, acos(clamp(direction.y, -1.0f, 1.0f)) / PI);
}

// Будує дотичний базис з похідних позиції та UV, тож мешу не потрібні збережені дотичні
float3x3 tangentFrame(float3 normal, float3 worldPos, float2 uv)
{
    float3 dp1 = ddx(worldPos);
    float3 dp2 = ddy(worldPos);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, normal);
    float3 dp1perp = cross(normal, dp1);

    float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;

    float scale = rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-20f));

    return float3x3(tangent * scale, -bitangent * scale, normal);
}

// Розпаковує нормаль з карти й посилює нахил
float3 unpackNormal(float4 packed, float scale)
{
    float3 normal = packed.rgb * 2.0f - 1.0f;
    normal.xy *= scale;

    return normal;
}

// Накладає нормаль деталей на основну
float3 blendNormalRNM(float3 n1, float3 n2)
{
    float3 t = n1 + float3(0.0f, 0.0f, 1.0f);
    float3 u = n2 * float3(-1.0f, -1.0f, 1.0f);

    return (t / t.z) * dot(t, u) - u;
}

// Зсув UV за картою висоти одним кроком
float2 parallaxOffset(float height, float amplitude, float3 viewTS)
{
    height = height * amplitude - amplitude / 2.0f;

    float3 v = normalize(viewTS);
    v.z += 0.42f;

    return height * (v.xy / v.z);
}

// Зчитує освітленість точки з одного каскаду карти тіней
float sampleCascade(int cascade, float3 worldPos, float3 normal, float slope)
{
	// Розмір текселя каскаду у світі: перший стовпець ортографічної матриці світла має довжину 2 / ширина
    float3 column = float3(lightViewProjection[cascade]._11, lightViewProjection[cascade]._21, lightViewProjection[cascade]._31);
    float texelWorld = 2.0f * shadowParams.x / length(column);

	// Зсув уздовж нормалі, більший під похилим світлом, не дає поверхні затіняти саму себе сходинками
    worldPos += normal * (texelWorld * NORMAL_OFFSET_TEXELS * slope);

    float4 shadowPos = mul(float4(worldPos, 1.0f), lightViewProjection[cascade]);

	// Переводить позицію зі простору світла в координати карти тіней
    float3 projected = shadowPos.xyz / shadowPos.w;
    float2 shadowCoord = float2(projected.x, -projected.y) * 0.5f + 0.5f;

	// Поза межами каскаду освітлення залишається без змін
    if (projected.z > 1.0f || any(saturate(shadowCoord) != shadowCoord)) return 1.0f;

	// Зсув підібрано під розмір текселя каскаду і збільшено на похилих поверхнях
    float depth = projected.z - cascadeBias[cascade] * (1.0f + 2.0f * slope);

	// Згладжує край тіні, усереднюючи результат порівняння по дев'яти текселях
    float lit = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            lit += ShadowMap.SampleCmpLevelZero(ShadowSampler,
                float3(shadowCoord + float2(x, y) * shadowParams.x, cascade), depth);
        }
    }

    return lit / 9.0f;
}

// Обчислює освітленість точки з боку світла за каскадною картою тіней
float calculateShadow(float3 worldPos, float3 normal, float3 toLight)
{
    float strength = shadowParams.w;
    if (strength <= 0.0f) return 1.0f;

	// Відстань уздовж осі камери визначає, який каскад покриває цей піксель
    float viewDepth = mul(float4(worldPos, 1.0f), view).z;

	// Номер каскаду дорівнює кількості перетнутих меж
    int cascade = (int) dot(step(cascadeSplits, viewDepth.xxxx), float4(1.0f, 1.0f, 1.0f, 1.0f));

    float facing = dot(normal, toLight);

	// Біля межі світла й тіні світло згасає плавно за кутом, ховаючи сходинчастий край карти тіней
    float selfShadow = smoothstep(0.0f, TERMINATOR_FADE, facing);

	// Далі за останній каскад і на відвернутих поверхнях карту читати нема потреби
    if (cascade >= SHADOW_CASCADE_COUNT || selfShadow <= 0.0f) return lerp(1.0f, selfShadow, strength);

    float slope = 1.0f - saturate(facing);

    float lit = sampleCascade(cascade, worldPos, normal, slope);

	// Ближче до межі каскаду підмішується наступний, щоб перехід не був помітним
    float blendBand = cascadeParams.y;

    if (blendBand > 0.0f && cascade + 1 < SHADOW_CASCADE_COUNT)
    {
        float blend = saturate((viewDepth - (cascadeSplits[cascade] - blendBand)) / blendBand);

        if (blend > 0.0f)
        {
            lit = lerp(lit, sampleCascade(cascade + 1, worldPos, normal, slope), blend);
        }
    }

    return lerp(1.0f, lit * selfShadow, strength);
}

// Навколишнє світло з неба в напрямку нормалі: найрозмитіший рівень неба близький до сферичних гармонік
float3 environmentDiffuse(float3 normal)
{
    if (environmentParams.w <= 0.0f) return ambientColor.rgb * environmentParams.x;

    float lod = max(environmentParams.z - 3.0f, 0.0f);

    return srgbToLinear(EnvironmentMap.SampleLevel(TextureSampler, directionToSky(normal), lod).rgb) * environmentParams.x;
}

// Відбиття неба з розмиттям за шорсткістю
float3 environmentSpecular(float3 reflected, float perceptualRoughness)
{
    if (environmentParams.w <= 0.0f) return ambientColor.rgb * environmentParams.y;

    float lod = perceptualRoughness * (1.7f - 0.7f * perceptualRoughness) * max(environmentParams.z - 3.0f, 0.0f);

    return srgbToLinear(EnvironmentMap.SampleLevel(TextureSampler, directionToSky(reflected), lod).rgb) * environmentParams.y;
}

// Властивості поверхні в одній точці
struct SurfaceData
{
    float3 albedo;
    float alpha;
    float metallic;
    float3 specular;
    float smoothness;
    float3 normalWS;
    float occlusion;
    float3 emission;
};

// Основний колір точки до освітлення; прототипний шейдер кладе текстуру поверх кольору за її альфою
float4 sampleBaseColor(float2 uv)
{
    float4 base = hasMap(MAP_BASE) ? BaseMap.Sample(TextureSampler, uv) : float4(1.0f, 1.0f, 1.0f, 1.0f);

#ifdef PROTOTYPE_BASE
    float4 color = float4(lerp(baseColor.rgb, base.rgb, base.a), baseColor.a);
#else
    float4 color = base * baseColor;
#endif

    return float4(srgbToLinear(color.rgb), color.a);
}

// Збирає властивості поверхні з карт і налаштувань матеріалу
SurfaceData initializeSurface(PS_INPUT input, float3 normalWS, float3 viewWS)
{
    SurfaceData surface;

    float2 uv = transformUv(input.texCoord, tilingOffset);

    float3x3 tbn = tangentFrame(normalWS, input.worldPos, uv);

	// Паралакс зсуває UV у бік погляду; у системі V рушія зсув по V має протилежний знак
    if (hasMap(MAP_HEIGHT))
    {
        float height = HeightMap.Sample(TextureSampler, uv).g;
        float2 offset = parallaxOffset(height, surfaceParams.w, mul(tbn, viewWS));

        uv += float2(offset.x, -offset.y);
    }

    float2 detailUv = transformUv(uv, detailTilingOffset);

    float4 albedoAlpha = sampleBaseColor(uv);

    surface.alpha = albedoAlpha.a;

    if (hasFeature(FEATURE_ALPHA_CLIP)) clip(surface.alpha - extraParams.y);

    float smoothnessSource = hasFeature(FEATURE_SMOOTHNESS_ALBEDO_ALPHA) ? albedoAlpha.a : 1.0f;

	// Металевість і гладкість з карти або з повзунків; карта відблиску в режимі Specular кольорова
    float4 gloss = hasMap(MAP_GLOSS) ? GlossMap.Sample(TextureSampler, uv) : float4(0.0f, 0.0f, 0.0f, 1.0f);

    if (hasFeature(FEATURE_SPECULAR_WORKFLOW))
    {
        surface.specular = hasMap(MAP_GLOSS) ? srgbToLinear(gloss.rgb) : srgbToLinear(specularColor.rgb);
        surface.metallic = 0.0f;
    }
    else
    {
        surface.specular = float3(0.0f, 0.0f, 0.0f);
        surface.metallic = hasMap(MAP_GLOSS) ? gloss.r : surfaceParams.x;
    }

    surface.smoothness = (hasMap(MAP_GLOSS) && !hasFeature(FEATURE_SMOOTHNESS_ALBEDO_ALPHA) ? gloss.a : smoothnessSource) * surfaceParams.y;

    surface.albedo = albedoAlpha.rgb;

    float3 normalTS = hasMap(MAP_NORMAL) ? unpackNormal(NormalMap.Sample(TextureSampler, uv), surfaceParams.z) : float3(0.0f, 0.0f, 1.0f);

	// Деталі змішуються за маскою: колір множиться, нормалі накладаються одна на одну
    float detailMask = hasMap(MAP_DETAIL_MASK) ? DetailMask.Sample(TextureSampler, uv).a : 1.0f;

    if (hasMap(MAP_DETAIL_ALBEDO))
    {
        float3 detailAlbedo = DetailAlbedoMap.Sample(TextureSampler, detailUv).rgb;
        detailAlbedo = 2.0f * detailAlbedo * extraParams.z - extraParams.z + 1.0f;

        surface.albedo *= lerp(float3(1.0f, 1.0f, 1.0f), detailAlbedo, detailMask);
    }

    if (hasMap(MAP_DETAIL_NORMAL))
    {
        float3 detailNormal = unpackNormal(DetailNormalMap.Sample(TextureSampler, detailUv), extraParams.w);

        normalTS = lerp(normalTS, blendNormalRNM(normalTS, detailNormal), detailMask);
    }

    surface.normalWS = hasMap(MAP_NORMAL) || hasMap(MAP_DETAIL_NORMAL) ? normalize(mul(normalTS, tbn)) : normalWS;

    float occlusion = hasMap(MAP_OCCLUSION) ? OcclusionMap.Sample(TextureSampler, uv).g : 1.0f;
    surface.occlusion = lerp(1.0f, occlusion, extraParams.x);

    surface.emission = float3(0.0f, 0.0f, 0.0f);

    if (hasFeature(FEATURE_EMISSION))
    {
        float3 emissionMap = hasMap(MAP_EMISSION) ? srgbToLinear(EmissionMap.Sample(TextureSampler, uv).rgb) : float3(1.0f, 1.0f, 1.0f);

        surface.emission = emissionMap * srgbToLinear(emissionColor.rgb);
    }

    return surface;
}

// Освітлює поверхню: сонце за моделлю GGX плюс небо
float4 shadeSurface(SurfaceData surface, float3 worldPos, float3 viewWS)
{
    float3 normal = surface.normalWS;

	// Відбивна здатність і кольори дифузного та дзеркального відбиття, як InitializeBRDFData
    float oneMinusReflectivity;
    float3 brdfDiffuse;
    float3 brdfSpecular;

    if (hasFeature(FEATURE_SPECULAR_WORKFLOW))
    {
        float reflectivity = max(surface.specular.r, max(surface.specular.g, surface.specular.b));

        oneMinusReflectivity = 1.0f - reflectivity;
        brdfDiffuse = surface.albedo * (float3(1.0f, 1.0f, 1.0f) - surface.specular);
        brdfSpecular = surface.specular;
    }
    else
    {
        oneMinusReflectivity = 0.96f - surface.metallic * 0.96f;
        brdfDiffuse = surface.albedo * oneMinusReflectivity;
        brdfSpecular = lerp(float3(0.04f, 0.04f, 0.04f), surface.albedo, surface.metallic);
    }

    float reflectivity = 1.0f - oneMinusReflectivity;
    float alpha = surface.alpha;

	// Попередньо помножена альфа гасить лише дифузне світло, тож відблиски лишаються яскравими
    if (hasFeature(FEATURE_TRANSPARENT) && flags.z == BLEND_PREMULTIPLY)
    {
        brdfDiffuse *= alpha;
        alpha = alpha * oneMinusReflectivity + reflectivity;
    }

    float perceptualRoughness = 1.0f - surface.smoothness;
    float roughness = max(perceptualRoughness * perceptualRoughness, 0.0078125f);
    float roughness2 = max(roughness * roughness, 6.103515625e-5f);
    float grazingTerm = saturate(surface.smoothness + reflectivity);
    float normalizationTerm = roughness * 4.0f + 2.0f;

	// Пряме світло сонця
    float3 color = float3(0.0f, 0.0f, 0.0f);
    float3 toLight = -lightDir;

    if (dot(toLight, toLight) > 0.000001f)
    {
        toLight = normalize(toLight);

        float nDotL = saturate(dot(normal, toLight));
        float shadow = hasFeature(FEATURE_RECEIVE_SHADOWS) ? calculateShadow(worldPos, normal, toLight) : 1.0f;

        float3 radiance = lightColor * (nDotL * shadow);
        float3 brdf = brdfDiffuse;

        if (hasFeature(FEATURE_SPECULAR_HIGHLIGHTS))
        {
            float3 halfDir = normalize(toLight + viewWS);
            float nDotH = saturate(dot(normal, halfDir));
            float lDotH = saturate(dot(toLight, halfDir));

            float d = nDotH * nDotH * (roughness2 - 1.0f) + 1.00001f;
            float specularTerm = roughness2 / ((d * d) * max(0.1f, lDotH * lDotH) * normalizationTerm);

            brdf += specularTerm * brdfSpecular;
        }

        color += brdf * radiance;
    }

	// Світло неба: розсіяне з усіх боків і відбите в напрямку погляду, приглушене затіненням
    float3 reflected = reflect(-viewWS, normal);
    float fresnel = pow(1.0f - saturate(dot(normal, viewWS)), 4.0f);

    float3 indirectDiffuse = environmentDiffuse(normal) * surface.occlusion;
    float3 indirectSpecular = (hasFeature(FEATURE_ENVIRONMENT_REFLECTIONS) ? environmentSpecular(reflected, perceptualRoughness) : environmentDiffuse(normal)) * surface.occlusion;

    float surfaceReduction = 1.0f / (roughness2 + 1.0f);

    color += indirectDiffuse * brdfDiffuse;
    color += surfaceReduction * indirectSpecular * lerp(brdfSpecular, grazingTerm.xxx, fresnel);

    color += surface.emission;

    if (!hasFeature(FEATURE_TRANSPARENT)) return float4(color, 1.0f);

	// Множення кадру на колір: де альфа менша, там кадр лишається світлішим
    if (flags.z == BLEND_MULTIPLY) color = lerp(float3(1.0f, 1.0f, 1.0f), color, alpha);

    return float4(color, alpha);
}

// Повний прохід Lit-шейдера для однієї точки
float4 litFragment(PS_INPUT input, bool isFrontFace)
{
    float3 normalWS = normalize(mul(input.normal, (float3x3) invTransModel));

	// Зворотний бік двобічної поверхні освітлюється зі свого боку
    if (!isFrontFace) normalWS = -normalWS;

    float3 viewWS = normalize(cameraPos - input.worldPos);

    SurfaceData surface = initializeSurface(input, normalWS, viewWS);

    return shadeSurface(surface, input.worldPos, viewWS);
}
