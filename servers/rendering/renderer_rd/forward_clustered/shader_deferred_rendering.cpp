/**************************************************************************/
/*  shader_deferred_rendering.cpp                                        */
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

#include "shader_deferred_rendering.h"

#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_server_default.h"

using namespace RendererRD;

//////////////////////////////////////////////////////////////////////////
// deferred rendering process shader
void DeferredRenderingRD::DeferredRenderingShaderData::set_code(const String &p_code) {
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
    actions.entry_point_stages["light"] = ShaderCompiler::STAGE_FRAGMENT;

	actions.uniforms = &uniforms;

	Error err = DeferredRenderingRD::get_singleton()->deferred_rendering_shader.compiler.compile(RS::SHADER_DEFERRED_PROCESS, code, &actions, path, gen_code);
	ERR_FAIL_COND_MSG(err != OK, "deferred_rendering_process shader compilation failed.");

	if (version.is_null()) {
		version = DeferredRenderingRD::get_singleton()->deferred_rendering_shader.shader.version_create();
	}

	DeferredRenderingRD::get_singleton()->deferred_rendering_shader.shader.version_set_code(version, gen_code.code, gen_code.uniforms, gen_code.stage_globals[ShaderCompiler::STAGE_VERTEX], gen_code.stage_globals[ShaderCompiler::STAGE_FRAGMENT], gen_code.defines);
	ERR_FAIL_COND(!DeferredRenderingRD::get_singleton()->deferred_rendering_shader.shader.version_is_valid(version));

	ubo_size = gen_code.uniform_total_size;
	ubo_offsets = gen_code.uniform_offsets;
	texture_uniforms = gen_code.texture_uniforms;

    pipeline.setup(DeferredRenderingRD::get_singleton()->deferred_rendering_shader.shader.version_get_shader(version, 0), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(1), 0);

	valid = true;
}

bool DeferredRenderingRD::DeferredRenderingShaderData::is_animated() const {
	return false;
}

bool DeferredRenderingRD::DeferredRenderingShaderData::casts_shadows() const {
	return false;
}

RS::ShaderNativeSourceCode DeferredRenderingRD::DeferredRenderingShaderData::get_native_source_code() const {
	return DeferredRenderingRD::get_singleton()->deferred_rendering_shader.shader.version_get_native_source_code(version);
}

DeferredRenderingRD::DeferredRenderingShaderData::~DeferredRenderingShaderData() {
	DeferredRenderingRD::get_singleton()->deferred_rendering_shader.shader.version_free(version);
}

//////////////////////////////////////////////////////////////////////////
// deferred rendering process material
bool DeferredRenderingRD::DeferredRenderingMaterialData::update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty) {
    uniform_set_updated = true;

    return update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set, DeferredRenderingRD::get_singleton()->deferred_rendering_shader.shader.version_get_shader(shader_data->version, 0), MATERIAL_UNIFORM_SET, true, true);
}

DeferredRenderingRD::DeferredRenderingMaterialData::~DeferredRenderingMaterialData() {
    free_parameters_uniform_set(uniform_set);
}

//////////////////////////////////////////////////////////////////////////
// deferred rendering process
void DeferredRenderingRD::init_deferred_rendering_shader() {
	MaterialStorage *material_storage = MaterialStorage::get_singleton();

    {
        material_storage->shader_set_data_request_function(MaterialStorage::SHADER_TYPE_DEFERRED_PROCESS, _create_pp_shader_funcs);
        material_storage->material_set_data_request_function(MaterialStorage::SHADER_TYPE_DEFERRED_PROCESS, _create_pp_material_funcs);
    }

    {
        String defines;
		defines += "\n#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS " + itos(RendererSceneRender::MAX_DIRECTIONAL_LIGHTS) + "\n";
        Vector<String> deferred_process_modes;
        deferred_process_modes.push_back("");
        deferred_rendering_shader.shader.initialize(deferred_process_modes, defines);
    }

    {
        ShaderCompiler::DefaultIdentifierActions actions;


		actions.renames["VIEW_MATRIX"] = "r_view_matrix";
		actions.renames["INV_VIEW_MATRIX"] = "r_inv_view_matrix";
		actions.renames["PROJECTION_MATRIX"] = "r_projection_matrix";
		actions.renames["INV_PROJECTION_MATRIX"] = "r_inv_projection_matrix";
		actions.renames["UV"] = "uv";
        actions.renames["NORMAL"] = "normal";

        actions.renames["COLOR"] = "color";
        actions.renames["POSITION"] = "position";
        actions.renames["LIGHT_DIR"] = "light_dir";
		actions.renames["LIGHT_COLOR"] = "light_color";
		actions.renames["LIGHT_IS_DIRECTIONAL"] = "light_is_directional";

        DeferredRenderingRD::get_singleton()->deferred_rendering_shader.compiler.initialize(actions);
    }

    {
        // default material and shader
        DeferredRenderingRD::get_singleton()->deferred_rendering_shader.default_shader = material_storage->shader_allocate();
        material_storage->shader_initialize(DeferredRenderingRD::get_singleton()->deferred_rendering_shader.default_shader);
        material_storage->shader_set_code(DeferredRenderingRD::get_singleton()->deferred_rendering_shader.default_shader, R"(
// Default deferred rendering process shader

shader_type deferred_process;

void process() {
    // get the color from the gbuffer
}
)");

        DeferredRenderingRD::get_singleton()->deferred_rendering_shader.default_material = material_storage->material_allocate();
        material_storage->material_initialize(DeferredRenderingRD::get_singleton()->deferred_rendering_shader.default_material);
        material_storage->material_set_shader(DeferredRenderingRD::get_singleton()->deferred_rendering_shader.default_material, deferred_rendering_shader.default_shader);

        DeferredRenderingMaterialData *material_data = static_cast<DeferredRenderingMaterialData *>(material_storage->material_get_data(DeferredRenderingRD::get_singleton()->deferred_rendering_shader.default_material, MaterialStorage::SHADER_TYPE_DEFERRED_PROCESS));
        DeferredRenderingRD::get_singleton()->deferred_rendering_shader.default_shader_rd = DeferredRenderingRD::get_singleton()->deferred_rendering_shader.shader.version_get_shader(material_data->shader_data->version, 0);
    }
}

void DeferredRenderingRD::free_deferred_rendering_shader() {
    MaterialStorage *material_storage = MaterialStorage::get_singleton();

    DeferredRenderingMaterialData *material_data = static_cast<DeferredRenderingMaterialData *>(material_storage->material_get_data(deferred_rendering_shader.default_material, MaterialStorage::SHADER_TYPE_DEFERRED_PROCESS));
    material_storage->shader_free(deferred_rendering_shader.default_shader);
    material_storage->material_free(deferred_rendering_shader.default_material);
    deferred_rendering_shader.shader.version_free(material_data->shader_data->version);
}

MaterialStorage::ShaderData *DeferredRenderingRD::_create_pp_shader_funcs() {
    DeferredRenderingShaderData *shader_data = memnew(DeferredRenderingShaderData);
    return shader_data;
}

MaterialStorage::MaterialData *DeferredRenderingRD::_create_pp_material_funcs(MaterialStorage::ShaderData *p_shader_data) {
	DeferredRenderingMaterialData *material_data = memnew(DeferredRenderingMaterialData);
	material_data->shader_data = static_cast<DeferredRenderingShaderData *>(p_shader_data);
	return material_data;
}

void DeferredRenderingRD::init() {
    init_deferred_rendering_shader();
}

void DeferredRenderingRD::free() {
    free_deferred_rendering_shader();
}

DeferredRenderingRD *DeferredRenderingRD::singleton = nullptr;

DeferredRenderingRD::DeferredRenderingRD() {
	singleton = this;
}

DeferredRenderingRD::~DeferredRenderingRD() {
    singleton = nullptr;
}
