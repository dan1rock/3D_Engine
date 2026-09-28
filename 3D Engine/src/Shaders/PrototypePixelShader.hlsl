// Прототипний матеріал: той самий Lit, але текстура лягає поверх кольору за своєю альфою, як сітка на сірому тлі
#define PROTOTYPE_BASE
#include "Lit.hlsli"

float4 main(PS_INPUT input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    return litFragment(input, isFrontFace);
}
