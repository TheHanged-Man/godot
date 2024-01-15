#[vertex]

#version 450
#define MAX_VIEWS 2

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

#VERSION_DEFINES

#define MAX_VIEWS 2

#include "../scene_data_inc.glsl"
#include "../light_data_inc.glsl"

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

layout(set = 1, binding = 1, std140) uniform DirectionalLights {
	DirectionalLightData data[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];
}
directional_lights;


void main() {
	SceneData scene_data = scene_data_block.data;
    highp mat4 r_view_matrix = scene_data.view_matrix;
    highp mat4 r_inv_view_matrix = scene_data.inv_view_matrix;
    highp mat4 r_projection_matrix = scene_data.projection_matrix;
    highp mat4 r_inv_projection_matrix = scene_data.inv_projection_matrix;

    vec3 color = texture(sampler2D(diffuse_texture, _sampler), uv, 0).rgb;
    vec3 normal = texture(sampler2D(normal_texture, _sampler), uv, 0).rgb;
    vec3 position = texture(sampler2D(position_texure, _sampler), uv, 0).rgb;

    {

#CODE : PROCESS

    }


    vec3 l_color = vec3(0.0);
    for (int i = 0; i < scene_data_block.data.directional_light_count; i++) {
        DirectionalLightData directional_light = directional_lights.data[i];
        vec3 light_dir = directional_light.direction;
        vec3 light_color = directional_light.color;
        bool light_is_directional = true;

        {

#CODE : LIGHT

        }

        if (directional_light.energy == 0.0) {
            break;
        }
        float lDotN = max(dot(normal, light_dir), 0.0);
        l_color += lDotN * light_color * directional_light.energy * color;
    }

    outColor = vec4(l_color, 1.0);
}
