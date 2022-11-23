#version 430

layout (location = 0) uniform sampler2D tex;
layout (location = 1) uniform int mode;
layout (location = 2) uniform vec2 win_size;

layout (location = 0) out vec4 fragColor;

in VS_OUT
{
    vec2 texcoord;
} fs_in;


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

vec4 image_abstraction() {
    // median blur
    int half_size = 2;
    vec4 color_sum = vec4(0);
    for (int i = -half_size; i <= half_size; ++i)
    {
        for (int j = -half_size; j <= half_size; ++j)
        {
            ivec2 coord = ivec2(gl_FragCoord.xy) + ivec2(i, j);
            color_sum += texelFetch(tex, coord, 0);
        }
    }
    int sample_count = (half_size * 2 + 1) * (half_size * 2 + 1);
    vec4 blurred = color_sum / sample_count;

    // quantization
    float nbins = 8.0;
    vec3 tex_color = floor(blurred.rgb * nbins) / nbins;
    return vec4(tex_color, 1.0) * diff_of_gaussian();
}

void main(void) {
    switch (mode) {
        case 0:
            fragColor = texture(tex, fs_in.texcoord);
            break;
        case 1:
            fragColor = image_abstraction();
            break;
    }
}
