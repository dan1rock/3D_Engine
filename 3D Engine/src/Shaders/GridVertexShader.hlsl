// Кількість каскадів карти тіней; те саме значення визначено в GlobalResources.h
#define SHADOW_CASCADE_COUNT 4

// Половина сторони квадрата сітки; дальше за неї камера однаково не бачить
#define GRID_EXTENT 250.0f

cbuffer constant: register(b0)
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
}

struct VS_OUTPUT
{
    float4 pos : SV_POSITION0;
    float3 worldPos : TEXCOORD0;
};

// Два трикутники квадрата, що лежить у площині XZ
static const float2 CORNERS[6] =
{
    float2(-1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, 1.0f),
    float2(-1.0f, -1.0f), float2(1.0f, 1.0f), float2(1.0f, -1.0f)
};

// Будує квадрат сітки під камерою без вершинного буфера - лише з номера вершини
VS_OUTPUT main(uint vertexId : SV_VertexID)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

	// Квадрат іде за камерою, тож сітка не має краю, а самі лінії рахуються зі світової позиції
    float2 corner = CORNERS[vertexId] * GRID_EXTENT;

    output.worldPos = float3(cameraPos.x + corner.x, 0.0f, cameraPos.z + corner.y);

    output.pos = mul(float4(output.worldPos, 1.0f), view);
    output.pos = mul(output.pos, projection);

    return output;
}
