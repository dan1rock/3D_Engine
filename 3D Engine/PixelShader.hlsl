struct PS_INPUT {
	float4 pos: SV_POSITION;
	float3 col: COLOR;
};

cbuffer constant: register(b0)
{
	row_major float4x4 world;
	row_major float4x4 view;
	row_major float4x4 projection;
	unsigned int time;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	return float4(input.col, 1.0f);
}
