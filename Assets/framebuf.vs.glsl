#version 430

layout (location = 0) in vec2 iv2win_pos;
layout (location = 1) in vec2 iv2tex_coord;

out VS_OUT
{
    vec2 texcoord;
} vs_out;

void main() {
    gl_Position = vec4(iv2win_pos, 1.0, 1.0);
    vs_out.texcoord = iv2tex_coord;
}