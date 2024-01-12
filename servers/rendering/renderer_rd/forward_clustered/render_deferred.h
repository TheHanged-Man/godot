#ifndef RENDER_DEFERRED_H
#define RENDER_DEFERRED_H

#include "core/templates/paged_allocator.h"
#include "servers/rendering/renderer_rd/cluster_builder_rd.h"
#include "servers/rendering/renderer_rd/effects/fsr2.h"
#include "servers/rendering/renderer_rd/effects/resolve.h"
#include "servers/rendering/renderer_rd/effects/ss_effects.h"
#include "servers/rendering/renderer_rd/effects/taa.h"
#include "servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.h"
#include "servers/rendering/renderer_rd/pipeline_cache_rd.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/shaders/forward_clustered/best_fit_normal.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/forward_clustered/scene_forward_clustered.glsl.gen.h"
#include "servers/rendering/renderer_rd/storage_rd/utilities.h"

#include "render_deferred_shader.h"
class RenderDeferred {



private:


	RenderDeferredShader deferred_shader;
	RID pipeline;
	RID shader;
	RID vertex_array;
	RID uniform_set0;

public:
	RID get_pipeline(RD::FramebufferFormatID p_fb_format);
	void set_pipeline(RID p_pipeline);
	void update_uniform_set0(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_shader);
	void render_color_buffer(RD::DrawListID p_draw_list, RD::FramebufferFormatID p_fb_format, Ref<RenderSceneBuffersRD> p_render_buffers);

	RenderDeferred();
};

#endif
