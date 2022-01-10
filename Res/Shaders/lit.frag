//glsl version 4.5
#version 450

//shader input
layout (location = 0) in vec3 inColour;

//output write
layout (location = 0) out vec4 outFragColour;

layout(set = 0, binding = 1) uniform  SceneData{
    vec4 fogColour; // w is for exponent
    vec4 fogDistances; //x for min, y for max, zw unused.
    vec4 ambientColour;
    vec4 sunlightDirection; //w for sun power
    vec4 sunlightColour;
} sceneData;


void main()
{
    outFragColour = vec4(inColour + sceneData.ambientColour.xyz,1.0f);
}
