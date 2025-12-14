#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/fwd.hpp>

// REMOVE this one day.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "graphics/input_graphics_sound_menu/input_graphics_sound_menu.hpp"
#include "graphics/draw_info/draw_info.hpp"

#include "input/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include "input/input_state/input_state.hpp"

#include "graphics/ui_render_suite_implementation/ui_render_suite_implementation.hpp"
#include "graphics/input_graphics_sound_menu/input_graphics_sound_menu.hpp"
#include "graphics/vertex_geometry/vertex_geometry.hpp"
#include "graphics/shader_standard/shader_standard.hpp"
#include "graphics/batcher/generated/batcher.hpp"
#include "graphics/shader_cache/shader_cache.hpp"
#include "graphics/fps_camera/fps_camera.hpp"
#include "graphics/window/window.hpp"
#include "graphics/colors/colors.hpp"
#include "graphics/ui/ui.hpp"

#include "system_logic/toolbox_engine/toolbox_engine.hpp"

#include "utility/fixed_frequency_loop/fixed_frequency_loop.hpp"
#include "utility/unique_id_generator/unique_id_generator.hpp"
#include "utility/logger/logger.hpp"

#include <iostream>

int main() {

    auto requested_shaders = {ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                              ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX};

    std::unordered_map<SoundType, std::string> sound_type_to_file = {
        {SoundType::UI_HOVER, "assets/sounds/hover.wav"},
        {SoundType::UI_CLICK, "assets/sounds/click.wav"},
        {SoundType::UI_SUCCESS, "assets/sounds/success.wav"},
    };

    ToolboxEngine tbx_engine("fps camera with geom", requested_shaders, sound_type_to_file);

    tbx_engine.fps_camera.fov.add_observer([&](const float &new_value) {
        tbx_engine.shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                            ShaderUniformVariable::CAMERA_TO_CLIP,
                                            tbx_engine.fps_camera.get_projection_matrix());
    });

    tbx_engine::register_input_graphics_sound_config_handlers(tbx_engine.configuration, tbx_engine.fps_camera,
                                                              tbx_engine.main_loop);

    tbx_engine.shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                        ShaderUniformVariable::RGBA_COLOR, glm::vec4(colors::cyan, 1));

    auto torus = vertex_geometry::generate_torus();

    tbx_engine.shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                        ShaderUniformVariable::CAMERA_TO_CLIP,
                                        tbx_engine.fps_camera.get_projection_matrix());

    tbx_engine.shader_cache.set_uniform(
        ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX, ShaderUniformVariable::ASPECT_RATIO,
        glm_utils::tuple_to_vec2(tbx_engine.window.get_aspect_ratio_in_simplest_terms()));

    std::function<void(double)> tick = [&](double dt) {
        tbx_engine.shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                            ShaderUniformVariable::CAMERA_TO_CLIP,
                                            tbx_engine.fps_camera.get_projection_matrix());

        tbx_engine.shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                            ShaderUniformVariable::WORLD_TO_CAMERA,
                                            tbx_engine.fps_camera.get_view_matrix());

        tbx_engine.shader_cache.set_uniform(
            ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX, ShaderUniformVariable::ASPECT_RATIO,
            glm_utils::tuple_to_vec2(tbx_engine.window.get_aspect_ratio_in_simplest_terms()));

        tbx_engine.update_camera_position_with_default_movement(dt);
        tbx_engine.process_and_queue_render_input_graphics_sound_menu();
        tbx_engine.update_active_mouse_mode(tbx_engine.igs_menu_active);
        tbx_engine.draw_chosen_engine_stats();

        tbx_engine.batcher.cwl_v_transformation_ubos_1024_with_solid_color_shader_batcher.queue_draw(torus);

        tbx_engine.batcher.cwl_v_transformation_ubos_1024_with_solid_color_shader_batcher.upload_ltw_matrices();
        tbx_engine.batcher.cwl_v_transformation_ubos_1024_with_solid_color_shader_batcher.draw_everything();
        tbx_engine.batcher.absolute_position_with_colored_vertex_shader_batcher.draw_everything();

        tbx_engine.sound_system.play_all_sounds();
    };

    std::function<bool()> termination = [&]() { return glfwWindowShouldClose(tbx_engine.window.glfw_window); };
    tbx_engine.start(tick, termination);

    return 0;
}
