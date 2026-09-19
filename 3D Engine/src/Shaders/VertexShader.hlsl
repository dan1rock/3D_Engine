// Кількість каскадів карти тіней; те саме значення визначено в GlobalResources.h
#define SHADOW_CASCADE_COUNT 4

struct VS_INPUT {
	float4 pos: POSITION0;
	float3 normal: NORMAL0;
	float2 texCoord: TEXCOORD0;
};

struct VS_OUTPUT {
	float4 pos: SV_POSITION0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD0;
    float3 cameraDir : TEXCOORD2;
    float3 lightDir : TEXCOORD3;
    float3 worldPos : TEXCOORD4;
};

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

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output = (VS_OUTPUT)0;

	output.pos = mul(input.pos, world);
    output.pos = mul(output.pos, model);
    output.cameraDir = normalize(output.pos.xyz - cameraPos.xyz);
    output.lightDir = normalize(lightPos.xyz - output.pos.xyz);

	// Каскад обирається для кожного пікселя окремо, тому далі передається світова позиція
    output.worldPos = output.pos.xyz;

	output.pos = mul(output.pos, view);
	output.pos = mul(output.pos, projection);

    output.normal = input.normal;
    output.texCoord = input.texCoord;
	return output;
}
