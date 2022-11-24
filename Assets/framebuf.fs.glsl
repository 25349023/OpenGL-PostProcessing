#version 430

layout (location = 0) uniform sampler2D tex;
layout (location = 1) uniform int mode;
layout (location = 2) uniform vec2 win_size;
layout (location = 3) uniform int comp_bar;
layout (location = 4) uniform sampler2D noise_tex;

layout (location = 0) out vec4 fragColor;

in VS_OUT
{
    vec2 texcoord;
} fs_in;


vec3 permute(vec3 x) { return mod(((x * 34.0) + 1.0) * x, 289.0); }

float snoise(vec2 v) {
    const vec4 C = vec4(0.211324865405187, 0.366025403784439,
                        -0.577350269189626, 0.024390243902439);
    vec2 i = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);
    vec2 i1;
    i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = mod(i, 289.0);
    vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0))
                     + i.x + vec3(0.0, i1.x, 1.0));
    vec3 m = max(0.5 - vec3(dot(x0, x0), dot(x12.xy, x12.xy),
                            dot(x12.zw, x12.zw)), 0.0);
    m = m * m;
    m = m * m;
    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);
    vec3 g;
    g.x = a0.x * x0.x + h.x * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

void draw_comparison_bar() {
    if (gl_FragCoord.x > comp_bar - 1 && gl_FragCoord.x < comp_bar + 1) {
        fragColor = vec4(1.0);
    }
}

vec4 diff_of_gaussian() {
    float sigma_e = 2.0f;
    float sigma_r = 2.8f;
    float phi = 3.4f;
    float tau = 0.99f;

    float twoSigmaESquared = 2.0 * sigma_e * sigma_e;
    float twoSigmaRSquared = 2.0 * sigma_r * sigma_r;
    int halfWidth = int(ceil(2.0 * sigma_r));

    vec2 sum = vec2(0.0);
    vec2 norm = vec2(0.0);
    int kernel_count = 0;
    for (int i = -halfWidth; i <= halfWidth; ++i) {
        for (int j = -halfWidth; j <= halfWidth; ++j) {
            float d = length(vec2(i, j));
            vec2 kernel = vec2(exp(-d * d / twoSigmaESquared),
                               exp(-d * d / twoSigmaRSquared));
            vec4 c = texture(tex, fs_in.texcoord + vec2(i, j) / win_size);
            vec2 L = vec2(0.299 * c.r + 0.587 * c.g + 0.114 * c.b);

            norm += 2.0 * kernel;
            sum += kernel * L;
        }
    }
    sum /= norm;

    float H = 100.0 * (sum.x - tau * sum.y);
    float edge = (H > 0.0) ? 1.0 : 2.0 * smoothstep(-2.0, 2.0, phi * H);

    return vec4(edge, edge, edge, 1.0);
}

vec4 median_blur(vec2 uv) {
    int half_size = 2;
    vec4 color_sum = vec4(0);
    for (int i = -half_size; i <= half_size; ++i)
    {
        for (int j = -half_size; j <= half_size; ++j)
        {
            ivec2 coord = ivec2(uv) + ivec2(i, j);
            color_sum += texelFetch(tex, coord, 0);
        }
    }
    int sample_count = (half_size * 2 + 1) * (half_size * 2 + 1);
    vec4 blurred = color_sum / sample_count;
    return blurred;
}

vec4 quantization(vec4 color) {
    float nbins = 8.0;
    vec3 tex_color = floor(color.rgb * nbins) / nbins;
    return vec4(tex_color, 1.0);
}

vec4 image_abstraction() {
    vec4 blurred = median_blur(gl_FragCoord.xy);
    return quantization(blurred) * diff_of_gaussian();
}

vec4 watercolor() {
    vec4 noise_color = texture(noise_tex, fs_in.texcoord);
    vec2 distorted_uv = gl_FragCoord.xy + (noise_color.xy) * 8;
    vec4 blurred = median_blur(distorted_uv);
    return quantization(blurred);
}

void main(void) {
    if (mode > 0 && gl_FragCoord.x < comp_bar + 1) {
        switch (mode) {
            case 1:
                fragColor = image_abstraction();
                break;
            case 2:
                fragColor = watercolor();
                break;
        }
        draw_comparison_bar();
    } else {
        fragColor = texture(tex, fs_in.texcoord);
    }
}
