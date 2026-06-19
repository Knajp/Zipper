#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTex;
layout(location = 2) flat in int fragTexID;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textures[];

void main()
{
    if(fragTexID >= 0)
    {
        outColor = texture(textures[fragTexID], fragTex);
    }
    else
    {
        outColor = vec4(fragColor, 1.0);
    }
}