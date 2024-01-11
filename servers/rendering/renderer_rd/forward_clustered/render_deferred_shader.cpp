#include "render_deferred_shader.h"
#include "render_deferred.h"
#include "scene_shader_forward_clustered.h"
#include "core/config/project_settings.h"
#include "core/math/math_defs.h"
#include "render_forward_clustered.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"

void RenderDeferredShader::ShaderData::set_code(const String &p_code) {

	code = p_code;
	valid = false;

	if (code.is_empty()) {
		return; //just invalid, but no error
	}

	ShaderCompiler::GeneratedCode gen_code;

	ShaderCompiler::IdentifierActions actions;

	//写入action数据
	{
		actions.entry_point_stages["process"] = ShaderCompiler::STAGE_FRAGMENT;
	}

	RenderDeferredShader *shader_singleton = (RenderDeferredShader *)RenderDeferredShader::singleton;

	if (version.is_null()) {
		version = shader_singleton->shader.version_create();
	}

	Error err = shader_singleton->compiler.compile(RS::SHADER_DEFERRED_PROCESS, code, &actions, path, gen_code);

#if 0
	print_line("**compiling shader:");
	print_line("**defines:\n");
	for (int i = 0; i < gen_code.defines.size(); i++) {
		print_line(gen_code.defines[i]);
	}

	HashMap<String, String>::Iterator el = gen_code.code.begin();
	while (el) {
		print_line("\n**code " + el->key + ":\n" + el->value);
		++el;
	}

	print_line("\n**uniforms:\n" + gen_code.uniforms);
	print_line("\n**vertex_globals:\n" + gen_code.stage_globals[ShaderCompiler::STAGE_VERTEX]);
	print_line("\n**fragment_globals:\n" + gen_code.stage_globals[ShaderCompiler::STAGE_FRAGMENT]);
#endif

	shader_singleton->shader.version_set_code(version, gen_code.code, gen_code.uniforms, gen_code.stage_globals[ShaderCompiler::STAGE_VERTEX], gen_code.stage_globals[ShaderCompiler::STAGE_FRAGMENT], gen_code.defines);
	ERR_FAIL_COND(!shader_singleton->shader.version_is_valid(version));


	RID shader_variant = shader_singleton->shader.version_get_shader(version, 0);



	//设置其他信息并写入管线信息
	//（此处不会创建管线对象）
	{
		RD::RenderPrimitive primitive_rd = RD::RenderPrimitive::RENDER_PRIMITIVE_TRIANGLES;
		RD::PipelineRasterizationState raster_state;
		RD::PipelineMultisampleState multisample_state;
		RD::PipelineDepthStencilState depth_stencil;
		RD::PipelineColorBlendState blend_state = RD::PipelineColorBlendState::create_disabled(1);


		pipeline_cache.setup(shader_variant, primitive_rd, raster_state, multisample_state, depth_stencil, blend_state);
	}


	valid = true;

}

RS::ShaderNativeSourceCode RenderDeferredShader::ShaderData::get_native_source_code() const {
	RenderDeferredShader *shader_singleton = (RenderDeferredShader *)RenderDeferredShader::singleton;

	return shader_singleton->shader.version_get_native_source_code(version);
}

bool RenderDeferredShader::ShaderData::is_animated() const {
	return false;
}

bool RenderDeferredShader::ShaderData::casts_shadows() const {
	return false;
}


RenderDeferredShader::ShaderData::ShaderData() :
		shader_list_element(this) {
}


RenderDeferredShader::ShaderData::~ShaderData() {
	RenderDeferredShader *shader_singleton = (RenderDeferredShader *)RenderDeferredShader::singleton;
	ERR_FAIL_NULL(shader_singleton);
	//pipeline variants will clear themselves if shader is gone
	if (version.is_valid()) {
		shader_singleton->shader.version_free(version);
	}
}

RendererRD::MaterialStorage::ShaderData *RenderDeferredShader::_create_shader_func() {
	ShaderData *shader_data = memnew(ShaderData);
	singleton->shader_list.add(&shader_data->shader_list_element);
	return shader_data;
}

void RenderDeferredShader::MaterialData::set_render_priority(int p_priority) {
	priority = p_priority - RS::MATERIAL_RENDER_PRIORITY_MIN; //8 bits
}

void RenderDeferredShader::MaterialData::set_next_pass(RID p_pass) {
	next_pass = p_pass;
}

bool RenderDeferredShader::MaterialData::update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty) {
	RenderDeferredShader *shader_singleton = (RenderDeferredShader *)RenderDeferredShader::singleton;

	return update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set, shader_singleton->shader.version_get_shader(shader_data->version, 0), RenderDeferredShader::MATERIAL_UNIFORM_SET, true, true);
}

RenderDeferredShader::MaterialData::~MaterialData() {
	free_parameters_uniform_set(uniform_set);
}

RendererRD::MaterialStorage::MaterialData *RenderDeferredShader::_create_material_func(ShaderData *p_shader) {
	MaterialData *material_data = memnew(MaterialData);
	material_data->shader_data = p_shader;
	//update will happen later anyway so do nothing.
	return material_data;
}

void RenderDeferredShader::init(const String p_defines) {
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	{
		enum ShaderGroup {
			SHADER_GROUP_BASE, // Always compiled at the beginning.
		};

		Vector<ShaderRD::VariantDefine> shader_versions;
		shader_versions.push_back(ShaderRD::VariantDefine(SHADER_GROUP_BASE, "\n#define LCH_MAKE_A_TRAVEL_HERE\n", true)); 

		shader.initialize(shader_versions, p_defines);

	}


	//compiler shader
	{
		ShaderCompiler::DefaultIdentifierActions actions;

		actions.renames["FIX0"] = "fix0";
		actions.renames["FIX1"] = "fix1";

		compiler.initialize(actions);
	}

	material_storage->shader_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_DEFERRED_PROCESS, _create_shader_funcs);
	material_storage->material_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_DEFERRED_PROCESS, _create_material_funcs);

}
RenderDeferredShader *RenderDeferredShader::singleton = nullptr;

RenderDeferredShader::RenderDeferredShader() {
	singleton = this;
}
