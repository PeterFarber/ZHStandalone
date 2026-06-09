#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

// Soft directional light from above + camera side.
void main() {
    const vec3 light_dir = normalize(vec3(0.25, 1.0, 0.35));
    const float ambient = 0.22;
    const float diffuse = 0.78;
    float ndotl = max(dot(normalize(fragNormal), light_dir), 0.0);
    vec3 lit = fragColor.rgb * (ambient + diffuse * ndotl);
    outColor = vec4(lit, fragColor.a);
}
