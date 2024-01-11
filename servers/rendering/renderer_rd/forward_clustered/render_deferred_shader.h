#ifndef RENDER_DEFERRED_SHADER_H
#define RENDER_DEFERRED_SHADER_H

#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/shaders/forward_clustered/render_deferred.glsl.gen.h"

class RenderDeferredShader {

public:
	enum {
		SCENE_UNIFORM_SET = 0,
		RENDER_PASS_UNIFORM_SET = 1,
		TRANSFORMS_UNIFORM_SET = 2,
		MATERIAL_UNIFORM_SET = 3,
	};


	static RenderDeferredShader *singleton;

	RenderDeferredShaderRD shader;
	ShaderCompiler compiler;
	


	struct ShaderData : public RendererRD::MaterialStorage::ShaderData {

		String code;
		bool valid = false;

		RID version;
		PipelineCacheRD pipeline_cache;

		SelfList<ShaderData> shader_list_element;

		Vector<uint32_t> ubo_offsets;
		Vector<ShaderCompiler::GeneratedCode::Texture> texture_uniforms;
		uint32_t ubo_size = 0;

		virtual void set_code(const String &p_Code);
		virtual bool is_animated() const;
		virtual bool casts_shadows() const;
		virtual RS::ShaderNativeSourceCode get_native_source_code() const;
		ShaderData();
		virtual ~ShaderData();
	};

	SelfList<ShaderData>::List shader_list;

	RendererRD::MaterialStorage::ShaderData *_create_shader_func();
	static RendererRD::MaterialStorage::ShaderData *_create_shader_funcs() {
		return static_cast<RenderDeferredShader *>(singleton)->_create_shader_func();
	}

	struct MaterialData : public RendererRD::MaterialStorage::MaterialData {
		ShaderData *shader_data = nullptr;
		RID uniform_set;
		uint64_t last_pass = 0;
		uint32_t index = 0;
		RID next_pass;
		uint8_t priority;
		virtual void set_render_priority(int p_priority);
		virtual void set_next_pass(RID p_pass);
		virtual bool update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty);
		virtual ~MaterialData();
	};

	//RendererRD::MaterialStorage::MaterialData *_create_material_func(ShaderData *p_shader);

	RendererRD::MaterialStorage::MaterialData *_create_material_func(ShaderData *p_shader);
	static RendererRD::MaterialStorage::MaterialData *_create_material_funcs(RendererRD::MaterialStorage::ShaderData *p_shader) {
		return static_cast<RenderDeferredShader *>(singleton)->_create_material_func(static_cast<ShaderData *>(p_shader));
	}



public:
	void init(const String p_defines = "");

	RenderDeferredShader();
};


#endif
