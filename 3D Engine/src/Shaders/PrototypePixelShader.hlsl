Texture2D Texture : register(t0);
sampler TextureSampler : register(s0);

Texture2D ShadowMap : register(t1);
SamplerComparisonState ShadowSampler : register(s1);

struct PS_INPUT
{
    float4 pos : SV_POSITION0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD0;
    float3 cameraDir : TEXCOORD2;
    float3 lightDir : TEXCOORD3;
    float4 shadowPos : TEXCOORD4;
};

cbuffer constant : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 model;
    row_major float4x4 invTransModel;
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 lightViewProjection;
    float3 cameraPos;
    float cameraPosPadding;
    float3 lightPos;
    float lightPosPadding;
    float3 lightColor;
    float lightColorPadding;
    float3 lightDir;
    float lightDirPadding;
    float4 shadowParams;
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

// Обчислює освітленість точки з боку світла за картою тіней
float calculateShadow(float4 shadowPos, float3 normal, float3 lightDir)
{
    float strength = shadowParams.w;
    if (strength <= 0.0f) return 1.0f;

	// Переводить позицію зі простору світла в координати карти тіней
    float3 projected = shadowPos.xyz / shadowPos.w;
    float2 shadowCoord = float2(projected.x, -projected.y) * 0.5f + 0.5f;

	// Поза межами карти тіней освітлення залишається без змін
    if (projected.z > 1.0f || any(saturate(shadowCoord) != shadowCoord)) return 1.0f;

	// Зсуває глибину тим сильніше, чим більший кут падіння світла на поверхню
    float slope = 1.0f - saturate(dot(normal, lightDir));
    float depth = projected.z - (shadowParams.y + shadowParams.z * slope);

	// Згладжує край тіні, усереднюючи результат порівняння по дев'яти текселях
    float lit = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            lit += ShadowMap.SampleCmpLevelZero(ShadowSampler, shadowCoord + float2(x, y) * shadowParams.x, depth);
        }
    }

    return lerp(1.0f, lit / 9.0f, strength);
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

    float shadow = calculateShadow(input.shadowPos, Normal, input.lightDir);

    float3 lighting = calculateLighting(ambient, diffuse, specular, shininess, lightColor, Normal, input.lightDir, input.cameraDir, shadow);

    float4 baseColor = color;

    if (isTextured)
    {
        float4 textureSample = Texture.Sample(TextureSampler, input.texCoord * textureScale);
        baseColor = color * (1.0f - textureSample.a) + textureSample * textureSample.a;
    }

    float4 result = baseColor * float4(lighting, 1.0f);

    return result;
}
