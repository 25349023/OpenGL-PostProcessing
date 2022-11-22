#version 430

layout (location = 0) out vec4 fragColor;

in VertexData
{
    vec3 N; // eye space normal
    vec3 L; // eye space light vector
    vec3 H; // eye space halfway vector
    vec2 texcoord;
} vertexData;

layout (location = 2) uniform sampler2D tex;

void main()
{
    vec3 texColor = texture(tex, vertexData.texcoord).rgb;
    fragColor = vec4(texColor, 1.0);
}