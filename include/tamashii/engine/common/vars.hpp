#pragma once
#include <tamashii/engine/common/var_system.hpp>

T_BEGIN_NAMESPACE
namespace var {
	extern Var program_name;
	// engine
	extern Var window_title;
	extern Var window_size;
	extern Var window_maximized;
	extern Var window_pos;
	extern Var window_thread;
	extern Var default_camera;
	extern Var headless;
	extern Var play_animation;
	extern Var cfg_filename;
	extern Var async_scene_loading;

	// renderer
	extern Var render_backend;
	extern Var render_thread;
	extern Var hide_default_gui;
	extern Var render_size;
	extern Var gpu_debug_info;
	extern Var default_font;
	extern Var work_dir;
	extern Var cache_dir;
	extern Var default_implementation;
	extern Var bg;
	extern Var light_overlay_color;
	extern Var max_fps;

	extern Var unique_model_refs;
	extern Var unique_material_refs;
	extern Var unique_light_refs;

	// backend
	extern Var vsync;
}
T_END_NAMESPACE
