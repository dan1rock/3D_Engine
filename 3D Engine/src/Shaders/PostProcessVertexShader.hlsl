struct VS_OUTPUT
{
    float4 pos : SV_POSITION0;
    float2 texCoord : TEXCOORD0;
};

// Будує трикутник, що перекриває весь екран, без вершинного буфера - лише з номера вершини
VS_OUTPUT main(uint vertexId : SV_VertexID)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

    output.texCoord = float2((vertexId << 1) & 2, vertexId & 2);
    output.pos = float4(output.texCoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return output;
}
