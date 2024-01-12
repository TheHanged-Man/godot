#include "render_deferred.h"
#include "render_deferred_shader.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/environment/post_process.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"


RID RenderDeferred::get_pipeline(RD::FramebufferFormatID p_fb_format) {
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

void RenderDeferred::set_pipeline(RID p_pipeline) {
	pipeline = p_pipeline;
}


void RenderDeferred::update_uniform_set0(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_shader) {
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

	uniform_set0 = RD::get_singleton()->uniform_set_create(uniforms, p_shader, 0);
}

void RenderDeferred::render_color_buffer(RD::DrawListID p_draw_list, RD::FramebufferFormatID p_fb_format, Ref<RenderSceneBuffersRD> p_render_buffers) {
	RID material;
	if (p_render_buffers->has_meta("post_process_material")) {
		material = p_render_buffers->get_meta("post_process_material");
	} else {
		material = RendererRD::PostProcessRD::get_singleton()->post_process_shader.default_material;
	}

	
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	RendererRD::PostProcessRD::PostProcessShaderData *shader_data = reinterpret_cast<RendererRD::PostProcessRD::PostProcessShaderData *>(material_storage->material_get_shader_data(material));
	RID p_shader = RendererRD::PostProcessRD::get_singleton()->post_process_shader.shader.version_get_shader(shader_data->version, 0);

	update_uniform_set0(p_render_buffers, p_shader);
	RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, uniform_set0, 0);

	RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, shader_data->pipeline.get_render_pipeline(RD::VertexFormatID(-1), p_fb_format));
	RD::get_singleton()->draw_list_draw(p_draw_list, false, 1, 6);
}

RenderDeferred::RenderDeferred() {

#if 0
	//use our shader
	{
		deferred_shader.init();
	}


	{
		String vertSrc;
		//create our vertex shader
		{
			vertSrc = String("");

		}


		String fragSrc;
		//create our frag shader
		{
			fragSrc = String("");
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
#endif

}

