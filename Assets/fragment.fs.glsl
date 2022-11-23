#version 430

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 fragNormColor;

in VertexData
{
    vec3 N; // eye space normal
    vec3 L; // eye space light vector
    vec3 H; // eye space halfway vector
    vec2 texcoord;
    vec3 normal;
} vertexData;

layout (location = 2) uniform sampler2D tex;

void main()
{
    vec3 texColor = texture(tex, vertexData.texcoord).rgb;
    fragColor = vec4(texColor, 1.0);
    fragNormColor = vec4(vertexData.normal, 1.0);
}