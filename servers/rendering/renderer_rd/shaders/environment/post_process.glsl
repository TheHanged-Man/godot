#[vertex]

#version 450
#define MAX_VIEWS 2
#include "../scene_data_inc.glsl"

vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);

vec2 uvs[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
);

layout(location = 0) out vec2 uv;

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	uv = uvs[gl_VertexIndex];
}

#[fragment]

#version 450

#define MAX_VIEWS 2

#include "../scene_data_inc.glsl"

struct DirectionalLight {
    vec3 position;
    vec3 color;
    float intensity;
} l1;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler _sampler;
layout(set = 0, binding = 1) uniform texture2D diffuse_texture;
layout(set = 0, binding = 2) uniform texture2D normal_texture;
layout(set = 0, binding = 3) uniform texture2D depth_texure;
layout(set = 0, binding = 4) uniform texture2D position_texure;

layout(set = 1, binding = 0, std140) uniform SceneDataBlock {
	SceneData data;
	SceneData prev_data;
}
scene_data_block;

vec3 diffuse(vec3 color, vec3 normal, vec3 position) {
    l1.position = (scene_data_block.data.view_matrix * vec4(l1.position, 1.0)).xyz;

    float lDotN = max(dot(normal, normalize(l1.position - position)), 0.0);
    return lDotN * l1.color * l1.intensity * color;
}

void main() {
    l1 = DirectionalLight(vec3(0.0, 10.0, 0.0), vec3(0.0, 1.0, 0.0), 1.0);
    vec3 color = texture(sampler2D(diffuse_texture, _sampler), uv, 0).rgb;
    vec3 normal = texture(sampler2D(normal_texture, _sampler), uv, 0).rgb;
    vec3 position = texture(sampler2D(position_texure, _sampler), uv, 0).rgb;
    float colorPercent = 0.9;
    float normalPercent = 0.1;

    {

#CODE : PROCESS

    }

    outColor = vec4(diffuse(color, normal, position), 1.0);
}
