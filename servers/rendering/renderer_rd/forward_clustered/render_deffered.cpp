#include "render_deffered.h"

RID RenderDeffered::get_pipeline(RD::FramebufferFormatID p_fb_format) {
	if (pipeline.is_valid())
		return pipeline;

	RenderingDeviceCommons::PipelineColorBlendState blends = RenderingDeviceCommons::PipelineColorBlendState::create_disabled(1);

	pipeline = RD::get_singleton()->render_pipeline_create(
		shader,
		p_fb_format,
		RD::VertexFormatID(-1),
		RD::RENDER_PRIMITIVE_TRIANGLES,
		RD::PipelineRasterizationState(),
		RenderingDeviceCommons::PipelineMultisampleState(),
		RenderingDeviceCommons::PipelineDepthStencilState(),
		blends);

	return pipeline;
}

void RenderDeffered::update_uniform_set0(Ref<RenderSceneBuffersRD> p_render_buffers) {
	Vector<RD::Uniform> uniforms;

	// bind 0
	{
		RenderingDeviceCommons::SamplerState sampler_state;
		
		RID sp = RD::get_singleton()->sampler_create(sampler_state);

		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
		u.binding = 0;
		u.append_id(sp);

		uniforms.push_back(u);
	}

	// bind 1
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 1;
		u.append_id(p_render_buffers->get_texture(RB_SCOPE_BUFFERS, RB_TEX_CUSTOM_COLOR));

		uniforms.push_back(u);
	}

	// bind 2
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 2;
		u.append_id(p_render_buffers->get_texture(RB_SCOPE_BUFFERS, RB_TEX_CUSTOM0));

		uniforms.push_back(u);
	}

	uniform_set0 = RD::get_singleton()->uniform_set_create(uniforms,shader,0);
}

void RenderDeffered::render_color_buffer(RD::DrawListID p_draw_list, RD::FramebufferFormatID p_fb_format, Ref<RenderSceneBuffersRD> p_render_buffers) {
	update_uniform_set0(p_render_buffers);
	RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list,uniform_set0,0);
	RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, get_pipeline(p_fb_format));
	RD::get_singleton()->draw_list_draw(p_draw_list, false, 1, 6);
}

RenderDeffered::RenderDeffered() {
	//create our vertex array
	
	{
		String vertSrc;
		//create our vertex shader
		{
			vertSrc = String("\n\
				#version 450\n       \
				vec2 positions[6] = vec2[](\n                           \
					vec2(-1.0, -1.0),\n                              \
					vec2(1.0, -1.0),\n                               \
					vec2(-1.0, 1.0),\n                               \
					vec2(-1.0, 1.0),\n                               \
					vec2(1.0, -1.0),\n                               \
					vec2(1.0, 1.0));\n                               \
				vec2 uvs[6] = vec2[](\n                           \
					vec2(0.0,0.0),\n                              \
					vec2(1.0, 0.0),\n                               \
					vec2(0.0, 1.0),\n                               \
					vec2(0.0, 1.0),\n                               \
					vec2(1.0, 0.0),\n                               \
					vec2(1.0, 1.0));\n                               \
				layout(location = 0) out vec2 uv;\n\
			layout(set = 0, binding = 0) uniform sampler _sampler;\n\
			layout(set = 0, binding = 1) uniform texture2D custom_color;   \n\
			layout(set = 0, binding = 2) uniform texture2D custom_tex0;   \n\
				void main() {   \n                                          \
				 gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n\
				 uv=uvs[gl_VertexIndex];\n\
		}\n");

		}


		String fragSrc;
		//create our frag shader
		{
			fragSrc = String("\n\
			#version 450	\n\
			layout(location = 0) in vec2 uv;\n\
			layout(location = 0) out vec4 outColor;\n\
			layout(set = 0, binding = 0) uniform sampler _sampler;\n\
			layout(set = 0, binding = 1) uniform texture2D custom_color;   \n\
			layout(set = 0, binding = 2) uniform texture2D custom_tex0;   \n\
			\
			void main() {\n\
			vec3 color = texture(sampler2D(custom_color,_sampler),uv,0).rgb;\n\
			vec3 normal = texture(sampler2D(custom_tex0,_sampler),uv,0).rgb;\n\
			outColor=vec4(0.9*color+0.1*normal,1.0);\n\
			}\n");
		}

		//Compile Shader
		{
			Ref<RDShaderSource> src;
			src.instantiate();
			src->set_stage_source(RD::SHADER_STAGE_VERTEX, vertSrc);
			src->set_stage_source(RD::SHADER_STAGE_FRAGMENT, fragSrc);

			Ref<RDShaderSPIRV> bytecode;
			bytecode.instantiate();
			for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
				String error;

				RenderingDeviceCommons::ShaderStage stage = RenderingDeviceCommons::ShaderStage(i);
				String source = src->get_stage_source(stage);

				if (!source.is_empty()) {
					Vector<uint8_t> spirv = RD::get_singleton()->shader_compile_spirv_from_source(stage, source, src->get_language(), &error, false);
					bytecode->set_stage_bytecode(stage, spirv);
					bytecode->set_stage_compile_error(stage, error);
				}
			}


			ERR_FAIL_COND_MSG(bytecode.is_null(), "RDShaderSPIRV bytecode is null ");

			Vector<RenderingDeviceCommons::ShaderStageSPIRVData> stage_data;
			for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
				RenderingDeviceCommons::ShaderStage stage = RenderingDeviceCommons::ShaderStage(i);
				RenderingDeviceCommons::ShaderStageSPIRVData sd;
				sd.shader_stage = stage;
				String error = bytecode->get_stage_compile_error(stage);
				print_line(error);
				ERR_FAIL_COND_MSG(!error.is_empty(), "Can't create a shader from an errored bytecode. Check errors in source bytecode.");
				sd.spirv = bytecode->get_stage_bytecode(stage);
				if (sd.spirv.is_empty()) {
					continue;
				}
				stage_data.push_back(sd);
			}
			shader = RD::get_singleton()->shader_create_from_spirv(stage_data);

		}

		//Create Pipeline
		{
			
		}
	}

}

