#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 u_direction;
uniform vec2 u_texelSize;
uniform float u_boost; // brightness multiplier -- 1.0 for the intermediate
                        // pass, higher for the final composite pass, since
                        // blurring spreads (and dims) the original energy.

out vec4 finalColor;

void main()
{
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec2 texOffset = u_texelSize * u_direction;

    vec4 result = texture(texture0, fragTexCoord) * weights[0];
    for (int i = 1; i < 5; i++) {
        vec2 offset = texOffset * float(i);
        result += texture(texture0, fragTexCoord + offset) * weights[i];
        result += texture(texture0, fragTexCoord - offset) * weights[i];
    }

    finalColor = result * fragColor * u_boost;
}