struct VS_INPUT
{
    float4 pos : POSITION0;
    float3 normal : NORMAL0;
    float2 texCoord : TEXCOORD0;
};

// Кількість каскадів карти тіней; те саме значення визначено в GlobalResources.h
#define SHADOW_CASCADE_COUNT 4

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

float4 main(VS_INPUT input) : SV_POSITION
{
    float4 pos = mul(input.pos, world);
    pos = mul(pos, model);

	// Проектує вершину у простір того каскаду, який зараз рендериться
    int cascade = (int) cascadeParams.x;

    return mul(pos, lightViewProjection[cascade]);
}
