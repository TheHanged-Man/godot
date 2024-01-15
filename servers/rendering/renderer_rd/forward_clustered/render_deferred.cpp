#include "render_deferred.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/forward_clustered/shader_deferred_rendering.h"

void RenderDeferred::update_texture_uniform_set(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_shader) {
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
		u.append_id(p_render_buffers->get_texture(RB_SCOPE_BUFFERS, RB_TEX_DEFER_NORMAL));

		uniforms.push_back(u);
	}

	// bind 3, depth texture
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 3;
		u.append_id(p_render_buffers->get_texture(RB_SCOPE_BUFFERS, RB_TEX_DEPTH));

		uniforms.push_back(u);
	}

	// bind position texture
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 4;
		u.append_id(p_render_buffers->get_texture(RB_SCOPE_BUFFERS, RB_TEX_POSITION));

		uniforms.push_back(u);
	}

	texture_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, p_shader, TEX_UNIFORM_SET);
}

// update render pass uniform set
void RenderDeferred::update_data_uniform_set(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_shader, RenderDataRD *p_render_data) {
	Vector<RD::Uniform> uniforms;

	// bind scene data
	{
		RD::Uniform u;
		u.binding = 0;
		u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.append_id(p_render_data->scene_data->get_uniform_buffer());
		uniforms.push_back(u);
	}
	{
		RD::Uniform u;
		u.binding = 1;
		u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.append_id(RendererRD::LightStorage::get_singleton()->get_directional_light_buffer());
		uniforms.push_back(u);
	}

	data_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, p_shader, DATA_UNIFORM_SET);
}

void RenderDeferred::render_color_buffer(RD::DrawListID p_draw_list, RD::FramebufferFormatID p_fb_format, Ref<RenderSceneBuffersRD> p_render_buffers, RenderDataRD *p_render_data) {
	RID material;
	if (p_render_buffers->has_meta("deferred_process_material")) {
		material = p_render_buffers->get_meta("deferred_process_material");
	} else {
		material = RendererRD::DeferredRenderingRD::get_singleton()->deferred_rendering_shader.default_material;
	}

	
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	RendererRD::DeferredRenderingRD::DeferredRenderingShaderData *shader_data = reinterpret_cast<RendererRD::DeferredRenderingRD::DeferredRenderingShaderData *>(material_storage->material_get_shader_data(material));
	RID p_shader = RendererRD::DeferredRenderingRD::get_singleton()->deferred_rendering_shader.shader.version_get_shader(shader_data->version, 0);

	update_texture_uniform_set(p_render_buffers, p_shader);
	update_data_uniform_set(p_render_buffers, p_shader, p_render_data);
	RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, texture_uniform_set, TEX_UNIFORM_SET);
	RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, data_uniform_set, DATA_UNIFORM_SET);

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

