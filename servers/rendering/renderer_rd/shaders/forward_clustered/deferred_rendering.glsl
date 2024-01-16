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
#define M_PI 3.14159265359

#include "../scene_data_inc.glsl"
#include "../light_data_inc.glsl"


uint sc_directional_soft_shadow_samples =0;


layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler _sampler;
layout(set = 0, binding = 1) uniform texture2D diffuse_texture;
layout(set = 0, binding = 2) uniform texture2D normal_texture;
layout(set = 0, binding = 3) uniform texture2D depth_texure;
layout(set = 0, binding = 4) uniform texture2D position_texure;

layout(set = 0, binding = 5) uniform sampler shadow_sampler;

layout(set = 0, binding = 6) uniform texture2D directional_shadow_atlas;



layout(set = 1, binding = 0, std140) uniform SceneDataBlock {
	SceneData data;
	SceneData prev_data;
}
scene_data_block;

layout(set = 1, binding = 1, std140) uniform DirectionalLights {
	DirectionalLightData data[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];
}
directional_lights;



#include "./deferred_lights_inc.glsl"



void main() {
	SceneData scene_data = scene_data_block.data;
    highp mat4 r_view_matrix = scene_data.view_matrix;
    highp mat4 r_inv_view_matrix = scene_data.inv_view_matrix;
    highp mat4 r_projection_matrix = scene_data.projection_matrix;
    highp mat4 r_inv_projection_matrix = scene_data.inv_projection_matrix;

    vec3 color = texture(sampler2D(diffuse_texture, _sampler), uv, 0).rgb;
    vec3 normal = texture(sampler2D(normal_texture, _sampler), uv, 0).rgb;
    vec3 position = texture(sampler2D(position_texure, _sampler), uv, 0).rgb;
    float shadow_depth = texture(sampler2D(directional_shadow_atlas, _sampler), uv, 0).x;


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

	uint shadow0 = 0;
	uint shadow1 = 0;

	for (uint i = 0; i < 8; i++) {
		if (i >= scene_data.directional_light_count) {
				break;
			}

		//No Mask
		if (directional_lights.data[i].bake_mode == LIGHT_BAKE_STATIC) {
				continue; // Statically baked light and object uses lightmap, skip
			}

		float shadow = 1.0;

		if (directional_lights.data[i].shadow_opacity > 0.001){
				float depth_z = -position.z;
				vec3 light_dir = directional_lights.data[i].direction;
				vec3 base_normal_bias = normalize(normal) * (1.0 - max(0.0, dot(light_dir, -normalize(normal))));

#define BIAS_FUNC(m_var, m_idx)                                                                 \
	m_var.xyz += light_dir * directional_lights.data[i].shadow_bias[m_idx];                     \
	vec3 normal_bias = base_normal_bias * directional_lights.data[i].shadow_normal_bias[m_idx]; \
	normal_bias -= light_dir * dot(light_dir, normal_bias);                                     \
	m_var.xyz += normal_bias;

				//Hard shadow
				{
					vec4 pssm_coord;
					float blur_factor;
					if (depth_z < directional_lights.data[i].shadow_split_offsets.x) {
						vec4 v = vec4(position, 1.0);

						BIAS_FUNC(v, 0)

						pssm_coord = (directional_lights.data[i].shadow_matrix1 * v);
						blur_factor = 1.0;
					} else if (depth_z < directional_lights.data[i].shadow_split_offsets.y) {
						vec4 v = vec4(position, 1.0);

						BIAS_FUNC(v, 1)

						pssm_coord = (directional_lights.data[i].shadow_matrix2 * v);
						// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
						blur_factor = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.y;
					} else if (depth_z < directional_lights.data[i].shadow_split_offsets.z) {
						vec4 v = vec4(position, 1.0);

						BIAS_FUNC(v, 2)

						pssm_coord = (directional_lights.data[i].shadow_matrix3 * v);
						// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
						blur_factor = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.z;
					} else {
						vec4 v = vec4(position, 1.0);

						BIAS_FUNC(v, 3)

						pssm_coord = (directional_lights.data[i].shadow_matrix4 * v);
						// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
						blur_factor = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.w;
					}

					pssm_coord /= pssm_coord.w;

					shadow = sample_directional_pcf_shadow(directional_shadow_atlas, scene_data.directional_shadow_pixel_size * directional_lights.data[i].soft_shadow_scale * (blur_factor + (1.0 - blur_factor) * float(directional_lights.data[i].blend_splits)), pssm_coord);
					
					if (directional_lights.data[i].blend_splits) {
						float pssm_blend;
						float blur_factor2;

						if (depth_z < directional_lights.data[i].shadow_split_offsets.x) {
							vec4 v = vec4(position, 1.0);
							BIAS_FUNC(v, 1)
							pssm_coord = (directional_lights.data[i].shadow_matrix2 * v);
							pssm_blend = smoothstep(directional_lights.data[i].shadow_split_offsets.x - directional_lights.data[i].shadow_split_offsets.x * 0.1, directional_lights.data[i].shadow_split_offsets.x, depth_z);
							// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
							blur_factor2 = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.y;
						} else if (depth_z < directional_lights.data[i].shadow_split_offsets.y) {
							vec4 v = vec4(position, 1.0);
							BIAS_FUNC(v, 2)
							pssm_coord = (directional_lights.data[i].shadow_matrix3 * v);
							pssm_blend = smoothstep(directional_lights.data[i].shadow_split_offsets.y - directional_lights.data[i].shadow_split_offsets.y * 0.1, directional_lights.data[i].shadow_split_offsets.y, depth_z);
							// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
							blur_factor2 = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.z;
						} else if (depth_z < directional_lights.data[i].shadow_split_offsets.z) {
							vec4 v = vec4(position, 1.0);
							BIAS_FUNC(v, 3)
							pssm_coord = (directional_lights.data[i].shadow_matrix4 * v);
							pssm_blend = smoothstep(directional_lights.data[i].shadow_split_offsets.z - directional_lights.data[i].shadow_split_offsets.z * 0.1, directional_lights.data[i].shadow_split_offsets.z, depth_z);
							// Adjust shadow blur with reference to the first split to reduce discrepancy between shadow splits.
							blur_factor2 = directional_lights.data[i].shadow_split_offsets.x / directional_lights.data[i].shadow_split_offsets.w;
						} else {
							pssm_blend = 0.0; //if no blend, same coord will be used (divide by z will result in same value, and already cached)
							blur_factor2 = 1.0;
						}

						pssm_coord /= pssm_coord.w;

						float shadow2 = sample_directional_pcf_shadow(directional_shadow_atlas, scene_data.directional_shadow_pixel_size * directional_lights.data[i].soft_shadow_scale * (blur_factor2 + (1.0 - blur_factor2) * float(directional_lights.data[i].blend_splits)), pssm_coord);
						shadow = mix(shadow, shadow2, pssm_blend);
					}
				}

		}

		if (i < 4) {
			shadow0 |= uint(clamp(shadow * 255.0, 0.0, 255.0)) << (i * 8);
		} else {
			shadow1 |= uint(clamp(shadow * 255.0, 0.0, 255.0)) << ((i - 4) * 8);
		}

		if(shadow<0.5)
			l_color.rgb=vec3(0);
	}


    outColor = vec4(l_color, 1.0);
}
