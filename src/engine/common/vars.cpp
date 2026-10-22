#include <tamashii/engine/common/vars.hpp>

T_USE_NAMESPACE
// vars that execute a special cmd
Var load_scene("load_scene", "", Var::Flag::STRING, "Load a scene on startup", "load_scene");

// vars that are just variables
Var tamashii::var::program_name("program_name", "Tamashii", Var::Flag::STRING | Var::Flag::INIT, "Name of the program", "set_var");

Var tamashii::var::window_title("window_title", "Tamashii", Var::Flag::STRING | Var::Flag::INIT, "Title of the main window", "set_var");
Var tamashii::var::window_size("window_size", "1280,720", Var::Flag::INTEGER | Var::Flag::INIT | Var::Flag::CONFIG_RDWR, "Main render window size width,height", "set_var");
Var tamashii::var::window_maximized("window_maximized", "0", Var::Flag::BOOL | Var::Flag::INIT | Var::Flag::CONFIG_RDWR, "Is main window maximized", "set_var");
Var tamashii::var::window_pos("window_pos", "0,0", Var::Flag::INTEGER | Var::Flag::INIT | Var::Flag::CONFIG_RDWR, "Main render window position", "set_var");
Var tamashii::var::window_thread("window_thread", "1", Var::Flag::BOOL | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Use a dedicated window thread", "set_var");
Var tamashii::var::default_camera("default_camera", DEFAULT_CAMERA_NAME, Var::Flag::STRING | Var::Flag::CONFIG_RD, "Camera that should be selected as default when a scene is loaded", "set_var");
Var tamashii::var::headless("headless", "0", Var::Flag::BOOL | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Start Tamashii without window", "set_var");
Var tamashii::var::play_animation("play_animation", "0", Var::Flag::BOOL | Var::Flag::INIT, "Play animation on startup", "set_var");
Var tamashii::var::cfg_filename("cfg_filename", "tamashii.cfg", Var::Flag::STRING | Var::Flag::INIT, "Name of the config file", "set_var");
Var tamashii::var::async_scene_loading("async_scene_loading", "1", Var::Flag::BOOL | Var::Flag::INIT, "Load scene async", "set_var");

Var tamashii::var::render_backend("render_backend", "vulkan", Var::Flag::STRING | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Render backend to use", "set_var");
Var tamashii::var::render_thread("render_thread", "0", Var::Flag::BOOL | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Use a dedicated rendering thread", "set_var");
Var tamashii::var::hide_default_gui("hide_default_gui", "0", Var::Flag::BOOL | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Do not show the default gui", "set_var");
Var tamashii::var::render_size("render_size", "400,400", Var::Flag::INTEGER | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Render size width,height; only used when in headless mode", "set_var");
Var tamashii::var::gpu_debug_info("gpu_debug_info", "0", Var::Flag::BOOL | Var::Flag::INIT, "Print GPU infos like available extentions", "set_var");
Var tamashii::var::default_font("default_font", "assets/fonts/Cousine-Regular.ttf", Var::Flag::STRING | Var::Flag::INIT | Var::Flag::CONFIG_RDWR, "Path to the font file", "set_var");
Var tamashii::var::work_dir("work_dir", ".", Var::Flag::STRING | Var::Flag::INIT, "Set the working dir", "set_var");
Var tamashii::var::cache_dir("cache_dir", "cache", Var::Flag::STRING | Var::Flag::INIT | Var::Flag::CONFIG_RDWR, "Dir for storing caches", "set_var");
Var tamashii::var::default_implementation("default_implementation", "Rasterizer", Var::Flag::STRING | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Startup implementation", "set_var");
Var tamashii::var::bg("bg", "30,30,30", Var::Flag::INTEGER | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Render background", "set_var");
Var tamashii::var::light_overlay_color("light_overlay_color", "220,220,220", Var::Flag::INTEGER | Var::Flag::INIT | Var::Flag::CONFIG_RD, "Light overlay color", "set_var");
Var tamashii::var::max_fps("max_fps", "0", Var::Flag::INTEGER | Var::Flag::INIT | Var::Flag::CONFIG_RDWR, "Set max fps", "set_var");

Var tamashii::var::unique_model_refs("unique_model_refs", "0", Var::Flag::BOOL | Var::Flag::INIT | Var::Flag::CONFIG_RD, "", "set_var");
Var tamashii::var::unique_material_refs("unique_material_refs", "0", Var::Flag::BOOL | Var::Flag::INIT | Var::Flag::CONFIG_RD, "", "set_var");
Var tamashii::var::unique_light_refs("unique_light_refs", "0", Var::Flag::BOOL | Var::Flag::INIT | Var::Flag::CONFIG_RD, "", "set_var");


Var tamashii::var::vsync("vsync", "1", Var::Flag::BOOL | Var::Flag::INIT | Var::Flag::CONFIG_RDWR, "Turn vsync ON/OFF", "set_var");