#include <glad/glad.h>
#include <GLFW/glfw3.h>

// REMOVE this one day.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "graphics/draw_info/draw_info.hpp"
#include "graphics/input_graphics_sound_menu/input_graphics_sound_menu.hpp"

#include "input/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include "input/input_state/input_state.hpp"

#include "utility/fixed_frequency_loop/fixed_frequency_loop.hpp"

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

#include "utility/unique_id_generator/unique_id_generator.hpp"
#include "utility/logger/logger.hpp"

#include <iostream>

std::optional<EKey> get_input_key_from_config_if_valid(InputState &input_state, Configuration &configuration,
                                                       const std::string &section_key) {
    auto key_value = configuration.get_value("input", section_key);
    if (key_value.has_value()) {
        auto key_value_str = key_value.value();
        if (input_state.is_valid_key_string(key_value_str)) {
            return input_state.key_str_to_key_enum.at(key_value_str);
        }
    }
    return std::nullopt;
}

const std::string config_value_slow_move = "slow_move";
const std::string config_value_fast_move = "fast_move";
const std::string config_value_forward = "forward";
const std::string config_value_left = "left";
const std::string config_value_back = "back";
const std::string config_value_right = "right";
const std::string config_value_up = "up";
const std::string config_value_down = "down";

std::unordered_map<std::string, EKey> movement_value_str_to_default_key = {{config_value_slow_move, EKey::LEFT_CONTROL},
                                                                           {config_value_fast_move, EKey::TAB},
                                                                           {config_value_forward, EKey::w},
                                                                           {config_value_left, EKey::a},
                                                                           {config_value_back, EKey::s},
                                                                           {config_value_right, EKey::d},
                                                                           {config_value_up, EKey::SPACE},
                                                                           {config_value_down, EKey::LEFT_SHIFT}};

EKey get_input_key_from_config_or_default_value(InputState &input_state, Configuration &configuration,
                                                const std::string &section_key) {
    auto opt_val = get_input_key_from_config_if_valid(input_state, configuration, section_key);
    return opt_val.value_or(movement_value_str_to_default_key.at(section_key));
}

glm::vec2 get_ndc_mouse_pos1(GLFWwindow *window, double xpos, double ypos) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    return {(2.0f * xpos) / width - 1.0f, 1.0f - (2.0f * ypos) / height};
}

glm::vec2 aspect_corrected_ndc_mouse_pos1(const glm::vec2 &ndc_mouse_pos, float x_scale) {
    return {ndc_mouse_pos.x * x_scale, ndc_mouse_pos.y};
}

class HUD {
  private:
    Batcher &batcher;
    InputState &input_state;
    Configuration &configuration;
    FPSCamera &fps_camera;
    UIRenderSuiteImpl &ui_render_suite;
    Window &window;

  public:
    HUD(Configuration &configuration, InputState &input_state, Batcher &batcher, FPSCamera &fps_camera,
        UIRenderSuiteImpl &ui_render_suite, Window &window)
        : batcher(batcher), input_state(input_state), configuration(configuration), fps_camera(fps_camera),
          ui_render_suite(ui_render_suite), window(window), ui(create_ui()) {}

    UI ui;
    int fps_element_id, pos_element_id;
    float average_fps;

    UI create_ui() {
        UI hud_ui(-1, batcher.absolute_position_with_colored_vertex_shader_batcher.object_id_generator);
        fps_element_id = hud_ui.add_textbox(
            "FPS", vertex_geometry::create_rectangle_from_top_right(glm::vec3(1, 1, 0), 0.2, 0.2), colors::black);
        pos_element_id = hud_ui.add_textbox(
            "POS", vertex_geometry::create_rectangle_from_bottom_left(glm::vec3(-1, -1, 0), 0.8, 0.4), colors::black);
        return hud_ui;
    }

    void process_and_queue_render_hud_ui_elements() {

        if (configuration.get_value("graphics", "show_pos").value_or("off") == "on") {
            ui.modify_text_of_a_textbox(pos_element_id, vec3_to_string(fps_camera.transform.get_translation()));
        }

        if (configuration.get_value("graphics", "show_fps").value_or("off") == "on") {
            std::ostringstream fps_stream;
            fps_stream << std::fixed << std::setprecision(1) << average_fps;
            ui.modify_text_of_a_textbox(fps_element_id, fps_stream.str());
        }

        auto ndc_mouse_pos =
            get_ndc_mouse_pos1(window.glfw_window, input_state.mouse_position_x, input_state.mouse_position_y);
        auto acnmp = aspect_corrected_ndc_mouse_pos1(ndc_mouse_pos, window.width_px / (float)window.height_px);

        process_and_queue_render_ui(acnmp, ui, ui_render_suite, input_state.get_just_pressed_key_strings(),
                                    input_state.is_just_pressed(EKey::BACKSPACE),
                                    input_state.is_just_pressed(EKey::ENTER),
                                    input_state.is_just_pressed(EKey::LEFT_MOUSE_BUTTON));
    }
};

void register_input_graphics_sound_config_handlers(Configuration &configuration, FPSCamera &fps_camera,
                                                   FixedFrequencyLoop &ffl) {
    configuration.register_config_handler("input", "mouse_sensitivity", [&](const std::string value) {
        float requested_sens;
        try {
            requested_sens = std::stof(value);
            fps_camera.change_active_sensitivity(requested_sens);
        } catch (const std::exception &) {
            std::cout << "sensivity is invalid" << std::endl;
        }
    });

    configuration.register_config_handler("graphics", "field_of_view", [&](const std::string value) {
        float fov, default_fov = 90;
        try {
            fov = std::stof(value);
            fps_camera.fov = fov;
        } catch (const std::exception &) {
            std::cout << "fov is invalid" << std::endl;
        }
    });

    configuration.register_config_handler("graphics", "max_fps", [&](const std::string value) {
        int max_fps;
        try {
            ffl.rate_limiter_enabled = true;
            max_fps = std::stoi(value);
        } catch (const std::exception &) {
            if (value == "inf") {
                ffl.rate_limiter_enabled = false;
            } else {
                std::cout << "max fps value couldn't be converted to an integer." << std::endl;
            }
        }
        ffl.update_rate_hz = max_fps;
        std::cout << "just set the update rate on the main tick" << std::endl;
    });
}

// TODO: need to extract this
void config_x_input_state_x_fps_camera_processing(FPSCamera &fps_camera, InputState &input_state,
                                                  Configuration &configuration, double dt) {
    fps_camera.process_input(
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_slow_move)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_fast_move)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_forward)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_left)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_back)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_right)),
        input_state.is_pressed(get_input_key_from_config_or_default_value(input_state, configuration, config_value_up)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_down)),
        dt);
}

#include <string>
#include <sstream>
#include <utility>

std::optional<std::pair<int, int>> extract_width_height_from_resolution(const std::string &resolution) {
    std::istringstream iss(resolution);
    int width = 0, height = 0;
    char delimiter = 'x';

    if (!(iss >> width))
        return std::nullopt;

    if (iss.peek() == delimiter)
        iss.ignore();

    if (!(iss >> height))
        return std::nullopt;

    std::pair<int, int> val = {width, height};
    return val;
}

int main() {

    ConsoleLogger main_logger;
    main_logger.set_name("main");

    FPSCamera fps_camera;
    // camera is initially frozen
    fps_camera.freeze_camera();

    bool start_in_fullscreen = false;
    bool start_with_mouse_captured = false;
    bool vsync = false;

    Configuration configuration("assets/config/user_cfg.ini");

    // NOTE: we use value or unless a user hasn't specified a resolution value
    std::string resolution = configuration.get_value("graphics", "resolution").value_or("1280x720");

    // NOTE: we use value or in the case that a user has specified a value but its improperly formatted.
    auto wh = extract_width_height_from_resolution(resolution).value_or({1280, 720});

    std::vector<std::string> on_off_options = {"on", "off"};
    std::function<bool(const std::string &)> convert_on_off_to_bool = [](const std::string &user_option) {
        if (user_option == "on") {
            return true;
        } else { // user option is false or an invalid string in either case return false
            return false;
        }
    };

    std::function<bool(const std::string &, const std::string &)> get_user_or_default_value =
        [&](const std::string &section, const std::string &key) { return convert_on_off_to_bool(key); };

    Window window(wh.first, wh.second, "mwe fps camera with geom", get_user_or_default_value("graphics", "fullscreen"),
                  start_with_mouse_captured, vsync);

    FixedFrequencyLoop ffl;

    InputState input_state;

    std::unordered_map<SoundType, std::string> sound_type_to_file = {
        {SoundType::UI_HOVER, "assets/sounds/hover.wav"},
        {SoundType::UI_CLICK, "assets/sounds/click.wav"},
        {SoundType::UI_SUCCESS, "assets/sounds/success.wav"},
    };
    SoundSystem sound_system(100, sound_type_to_file);

    std::vector<ShaderType> requested_shaders = {ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                                 ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX};

    ShaderCache shader_cache(requested_shaders);
    Batcher batcher(shader_cache);

    fps_camera.fov.add_observer([&](const float &new_value) {
        shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                 ShaderUniformVariable::CAMERA_TO_CLIP,
                                 fps_camera.get_projection_matrix(window.width_px, window.height_px));
    });

    register_input_graphics_sound_config_handlers(configuration, fps_camera, ffl);

    UIRenderSuiteImpl ui_render_suite(batcher);
    HUD hud(configuration, input_state, batcher, fps_camera, ui_render_suite, window);
    InputGraphicsSoundMenu input_graphics_sound_menu(window, input_state, batcher, sound_system, configuration);

    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                             ShaderUniformVariable::RGBA_COLOR, glm::vec4(colors::cyan, 1));

    GLFWLambdaCallbackManager glcm =
        create_default_glcm_for_input_and_camera(input_state, fps_camera, window, shader_cache);

    auto ball = vertex_geometry::generate_torus();
    Transform ball_transform;

    glm::mat4 identity = glm::mat4(1);
    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                             ShaderUniformVariable::CAMERA_TO_CLIP,
                             fps_camera.get_projection_matrix(window.width_px, window.height_px));

    shader_cache.set_uniform(ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX, ShaderUniformVariable::ASPECT_RATIO,
                             glm::vec2(window.height_px / (float)window.width_px, 1));

    // RateLimitedConsoleLogger tick_logger(1);
    ConsoleLogger tick_logger;
    tick_logger.disable_all_levels();
    std::function<void(double)> tick = [&](double dt) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                 ShaderUniformVariable::CAMERA_TO_CLIP,
                                 fps_camera.get_projection_matrix(window.width_px, window.height_px));

        shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                 ShaderUniformVariable::WORLD_TO_CAMERA, fps_camera.get_view_matrix());

        std::vector<unsigned int> ltw_indices(ball.xyz_positions.size(), 0);
        batcher.cwl_v_transformation_ubos_1024_with_solid_color_shader_batcher.queue_draw(
            0, ball.indices, ball.xyz_positions, ltw_indices);

        batcher.cwl_v_transformation_ubos_1024_with_solid_color_shader_batcher.ltw_matrices[0] =
            ball_transform.get_transform_matrix();

        potentially_switch_between_menu_and_3d_view(input_state, input_graphics_sound_menu, fps_camera, window);
        hud.process_and_queue_render_hud_ui_elements();

        if (input_graphics_sound_menu.enabled) {
            input_graphics_sound_menu.process_and_queue_render_menu(window, input_state, ui_render_suite);
        }

        batcher.cwl_v_transformation_ubos_1024_with_solid_color_shader_batcher.upload_ltw_matrices();
        batcher.cwl_v_transformation_ubos_1024_with_solid_color_shader_batcher.draw_everything();
        batcher.absolute_position_with_colored_vertex_shader_batcher.draw_everything();

        sound_system.play_all_sounds();

        glfwSwapBuffers(window.glfw_window);
        glfwPollEvents();

        // tick_logger.tick();

        TemporalBinarySignal::process_all();
    };

    std::function<bool()> termination = [&]() { return glfwWindowShouldClose(window.glfw_window); };

    std::function<void(IterationStats)> loop_stats_function = [&](IterationStats is) {
        hud.average_fps = is.measured_frequency_hz;
    };

    ffl.start(120, tick, termination, loop_stats_function);

    return 0;
}
