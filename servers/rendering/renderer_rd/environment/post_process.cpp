/**************************************************************************/
/*  post_process.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "post_process.h"

#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_server_default.h"

using namespace RendererRD;

//////////////////////////////////////////////////////////////////////////
// post process shader
void PostProcessRD::PostProcessShaderData::set_code(const String &p_code) {
	//compile

	code = p_code;
	valid = false;
	ubo_size = 0;
	uniforms.clear();

	if (code.is_empty()) {
		return; //just invalid, but no error
	}

	ShaderCompiler::GeneratedCode gen_code;
	ShaderCompiler::IdentifierActions actions;
	actions.entry_point_stages["process"] = ShaderCompiler::STAGE_FRAGMENT;

	actions.uniforms = &uniforms;

	Error err = PostProcessRD::get_singleton()->post_process_shader.compiler.compile(RS::SHADER_POST_PROCESS, code, &actions, path, gen_code);
	ERR_FAIL_COND_MSG(err != OK, "post_process shader compilation failed.");

	if (version.is_null()) {
		version = PostProcessRD::get_singleton()->post_process_shader.shader.version_create();
	}

	PostProcessRD::get_singleton()->post_process_shader.shader.version_set_code(version, gen_code.code, gen_code.uniforms, gen_code.stage_globals[ShaderCompiler::STAGE_VERTEX], gen_code.stage_globals[ShaderCompiler::STAGE_FRAGMENT], gen_code.defines);
	ERR_FAIL_COND(!PostProcessRD::get_singleton()->post_process_shader.shader.version_is_valid(version));

	ubo_size = gen_code.uniform_total_size;
	ubo_offsets = gen_code.uniform_offsets;
	texture_uniforms = gen_code.texture_uniforms;

    pipeline.setup(PostProcessRD::get_singleton()->post_process_shader.shader.version_get_shader(version, 0), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(1), 0);

	valid = true;
}

bool PostProcessRD::PostProcessShaderData::is_animated() const {
	return false;
}

bool PostProcessRD::PostProcessShaderData::casts_shadows() const {
	return false;
}

RS::ShaderNativeSourceCode PostProcessRD::PostProcessShaderData::get_native_source_code() const {
	return PostProcessRD::get_singleton()->post_process_shader.shader.version_get_native_source_code(version);
}

PostProcessRD::PostProcessShaderData::~PostProcessShaderData() {
	PostProcessRD::get_singleton()->post_process_shader.shader.version_free(version);
}

//////////////////////////////////////////////////////////////////////////
// post process material
bool PostProcessRD::PostProcessMaterialData::update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty) {
    uniform_set_updated = true;

    return update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set, PostProcessRD::get_singleton()->post_process_shader.shader.version_get_shader(shader_data->version, 0), MATERIAL_UNIFORM_SET, true, true);
}

PostProcessRD::PostProcessMaterialData::~PostProcessMaterialData() {
    free_parameters_uniform_set(uniform_set);
}

//////////////////////////////////////////////////////////////////////////
// post process
void PostProcessRD::init_post_process_shader() {
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

    {
        material_storage->shader_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_POST_PROCESS, _create_pp_shader_funcs);
        material_storage->material_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_POST_PROCESS, _create_pp_material_funcs);
    }

    {
        String defines = "\n";
        Vector<String> post_process_modes;
        post_process_modes.push_back("");
        post_process_shader.shader.initialize(post_process_modes, defines);
    }

    {
        ShaderCompiler::DefaultIdentifierActions actions;

        actions.renames["COLOR_PERCENT"] = "colorPercent";
        actions.renames["NORMAL_PERCENT"] = "normalPercent";

        PostProcessRD::get_singleton()->post_process_shader.compiler.initialize(actions);
    }

    {
        // default material and shader
        PostProcessRD::get_singleton()->post_process_shader.default_shader = material_storage->shader_allocate();
        material_storage->shader_initialize(PostProcessRD::get_singleton()->post_process_shader.default_shader);
        material_storage->shader_set_code(PostProcessRD::get_singleton()->post_process_shader.default_shader, R"(
// Default post process shader

shader_type post_process;

void process() {
    COLOR_PERCENT = 0.9;
    NORMAL_PERCENT = 0.1;
}
)");

        PostProcessRD::get_singleton()->post_process_shader.default_material = material_storage->material_allocate();
        material_storage->material_initialize(PostProcessRD::get_singleton()->post_process_shader.default_material);
        material_storage->material_set_shader(PostProcessRD::get_singleton()->post_process_shader.default_material, post_process_shader.default_shader);

        PostProcessMaterialData *material_data = static_cast<PostProcessMaterialData *>(material_storage->material_get_data(PostProcessRD::get_singleton()->post_process_shader.default_material, RendererRD::MaterialStorage::SHADER_TYPE_POST_PROCESS));
        PostProcessRD::get_singleton()->post_process_shader.default_shader_rd = PostProcessRD::get_singleton()->post_process_shader.shader.version_get_shader(material_data->shader_data->version, 0);
    }
}

void PostProcessRD::free_post_process_shader() {
    RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

    PostProcessMaterialData *material_data = static_cast<PostProcessMaterialData *>(material_storage->material_get_data(post_process_shader.default_material, RendererRD::MaterialStorage::SHADER_TYPE_POST_PROCESS));
    material_storage->shader_free(post_process_shader.default_shader);
    material_storage->material_free(post_process_shader.default_material);
    post_process_shader.shader.version_free(material_data->shader_data->version);
}

RendererRD::MaterialStorage::ShaderData *PostProcessRD::_create_pp_shader_funcs() {
    PostProcessShaderData *shader_data = memnew(PostProcessShaderData);
    return shader_data;
}

RendererRD::MaterialStorage::MaterialData *PostProcessRD::_create_pp_material_funcs(RendererRD::MaterialStorage::ShaderData *p_shader_data) {
	PostProcessMaterialData *material_data = memnew(PostProcessMaterialData);
	material_data->shader_data = static_cast<PostProcessShaderData *>(p_shader_data);
	return material_data;
}

void PostProcessRD::init() {
    init_post_process_shader();
}

void PostProcessRD::free() {
    free_post_process_shader();
}

RendererRD::PostProcessRD *PostProcessRD::singleton = nullptr;

PostProcessRD::PostProcessRD() {
	singleton = this;
}

PostProcessRD::~PostProcessRD() {
    singleton = nullptr;
}
