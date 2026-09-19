// Кількість каскадів карти тіней; те саме значення визначено в GlobalResources.h
#define SHADOW_CASCADE_COUNT 4

Texture2D Texture : register(t0);
sampler TextureSampler : register(s0);

Texture2DArray ShadowMap : register(t1);
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
};

cbuffer material : register(b1)
{
    float ambient;
    float diffuse;
    float specular;
    float shininess;
    float textureScale;
    float4 color;
    bool isTextured;
};

// Зчитує освітленість точки з одного каскаду карти тіней
float sampleCascade(int cascade, float3 worldPos, float slope)
{
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
float calculateShadow(float3 worldPos, float3 normal, float3 lightDir)
{
    float strength = shadowParams.w;
    if (strength <= 0.0f) return 1.0f;

	// Відстань уздовж осі камери визначає, який каскад покриває цей піксель
    float viewDepth = mul(float4(worldPos, 1.0f), view).z;

	// Номер каскаду дорівнює кількості перетнутих меж
    int cascade = (int) dot(step(cascadeSplits, viewDepth.xxxx), float4(1.0f, 1.0f, 1.0f, 1.0f));

	// Далі за останній каскад тіні не будуються
    if (cascade >= SHADOW_CASCADE_COUNT) return 1.0f;

    float slope = 1.0f - saturate(dot(normal, lightDir));

    float lit = sampleCascade(cascade, worldPos, slope);

	// Ближче до межі каскаду підмішується наступний, щоб перехід не був помітним
    float blendBand = cascadeParams.y;

    if (blendBand > 0.0f && cascade + 1 < SHADOW_CASCADE_COUNT)
    {
        float blend = saturate((viewDepth - (cascadeSplits[cascade] - blendBand)) / blendBand);

        if (blend > 0.0f)
        {
            lit = lerp(lit, sampleCascade(cascade + 1, worldPos, slope), blend);
        }
    }

    return lerp(1.0f, lit, strength);
}

float3 calculateLighting(float ambient, float diffuse, float specular, float shininess, float3 lightColor, float3 normal, float3 lightDir, float3 cameraDir, float shadow)
{
	//AMBIENT LIGHT
    float3 ambient_light = ambient * lightColor;

	//DIFFUSE LIGHT
    float amount_diffuse_light = (dot(lightDir, normal) + 1.0f) * 0.5f;
    float3 diffuse_light = diffuse * lightColor * amount_diffuse_light;

	//SPECULAR LIGHT
    float3 reflected_light = reflect(lightDir, normal);
    float amount_specular_light = pow(max(0.0, dot(reflected_light, cameraDir)), shininess);

    float3 specular_light = specular * amount_specular_light * lightColor;

	//SHADOW
    float3 final_light = ambient_light + (diffuse_light + specular_light) * shadow;

    return final_light;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 Normal = normalize(mul(input.normal, (float3x3) invTransModel));

    float shadow = calculateShadow(input.worldPos, Normal, input.lightDir);

    float3 lighting = calculateLighting(ambient, diffuse, specular, shininess, lightColor, Normal, input.lightDir, input.cameraDir, shadow);

    float4 baseColor = color;

    if (isTextured)
    {
        baseColor *= Texture.Sample(TextureSampler, input.texCoord * textureScale);
    }

    float4 result = baseColor * float4(lighting, 1.0f);

    return result;
}
