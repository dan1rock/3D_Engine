// Lit-матеріал: карти поверхні, освітлення GGX і світло неба
#include "Lit.hlsli"

float4 main(PS_INPUT input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    return litFragment(input, isFrontFace);
}
