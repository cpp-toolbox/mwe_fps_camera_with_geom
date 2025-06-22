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

int main() {

    bool menu_enabled = true;

    ConsoleLogger main_logger;
    main_logger.set_name("main");

    FPSCamera fps_camera;
    // camera is initially frozen
    fps_camera.freeze_camera();

    unsigned int window_width_px = 700, window_height_px = 700;
    bool start_in_fullscreen = false;
    bool start_with_mouse_captured = false;
    bool vsync = false;

    main_logger.debug("1");

    Window window(window_width_px, window_height_px, "mwe fps camera with geom", start_in_fullscreen,
                  start_with_mouse_captured, vsync);

    InputState input_state;

    std::unordered_map<SoundType, std::string> sound_type_to_file = {
        {SoundType::UI_HOVER, "assets/sounds/hover.wav"},
        {SoundType::UI_CLICK, "assets/sounds/click.wav"},
        {SoundType::UI_SUCCESS, "assets/sounds/success.wav"},
    };
    SoundSystem sound_system(100, sound_type_to_file);

    std::vector<ShaderType> requested_shaders = {ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                                 ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX};

    main_logger.debug("2");

    ShaderCache shader_cache(requested_shaders);
    Batcher batcher(shader_cache);

    fps_camera.fov.add_observer([&](const float &new_value) {
        shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                 ShaderUniformVariable::CAMERA_TO_CLIP,
                                 fps_camera.get_projection_matrix(window.width_px, window.height_px));
    });

    Configuration configuration("assets/config/user_cfg.ini");

    configuration.register_config_handler("graphics", "field_of_view", [&](const std::string value) {
        float fov, default_fov = 90;
        try {
            fov = std::stof(value);
        } catch (const std::exception &) {
            fov = default_fov;
        }
        fps_camera.fov = fov;
        std::cout << "just set the fov" << std::endl;
    });

    for (const auto &pair : movement_value_str_to_default_key) {
        const std::string &value_str = pair.first;
        EKey default_key = pair.second;

        if (!configuration.has_value("input", value_str)) {
            configuration.set_value("input", value_str, input_state.key_enum_to_object.at(default_key)->string_repr);
        }
    }

    UIRenderSuiteImpl ui_render_suite(batcher);

    main_logger.debug("2.5");

    InputGraphicsSoundMenu input_graphics_sound_menu(window, input_state, batcher, sound_system, configuration);

    main_logger.debug("3");

    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe mode
    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                             ShaderUniformVariable::RGBA_COLOR, glm::vec4(colors::cyan, 1));

    std::function<void(unsigned int)> char_callback = [](unsigned int codepoint) {};
    std::function<void(int, int, int, int)> key_callback = [&](int key, int scancode, int action, int mods) {
        input_state.glfw_key_callback(key, scancode, action, mods);
    };
    std::function<void(double, double)> mouse_pos_callback = [&](double xpos, double ypos) {
        fps_camera.mouse_callback(xpos, ypos);
        input_state.glfw_cursor_pos_callback(xpos, ypos);
    };
    std::function<void(int, int, int)> mouse_button_callback = [&](int button, int action, int mods) {
        input_state.glfw_mouse_button_callback(button, action, mods);
    };
    std::function<void(int, int)> frame_buffer_size_callback = [&](int width, int height) {
        // this gets called whenever the window changes size, because the framebuffer automatically
        // changes size, that is all done in glfw's context, then we need to update opengl's size.
        std::cout << "framebuffersize callback called, width" << width << "height: " << height << std::endl;
        glViewport(0, 0, width, height);
        window.width_px = width;
        window.height_px = height;

        shader_cache.set_uniform(ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX, ShaderUniformVariable::ASPECT_RATIO,
                                 glm::vec2(height / (float)width, 1));
    };
    GLFWLambdaCallbackManager glcm(window.glfw_window, char_callback, key_callback, mouse_pos_callback,
                                   mouse_button_callback, frame_buffer_size_callback);

    auto ball = vertex_geometry::generate_torus();
    Transform ball_transform;

    glm::mat4 identity = glm::mat4(1);
    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                             ShaderUniformVariable::CAMERA_TO_CLIP,
                             fps_camera.get_projection_matrix(window_width_px, window_height_px));

    shader_cache.set_uniform(ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX, ShaderUniformVariable::ASPECT_RATIO,
                             glm::vec2(window.height_px / (float)window.width_px, 1));

    GLuint ltw_matrices_gl_name;
    BoundedUniqueIDGenerator ltw_id_generator(1024);
    glm::mat4 ltw_matrices[1024];

    // initialize all matrices to identity matrices
    for (int i = 0; i < 1024; ++i) {
        ltw_matrices[i] = identity;
    }

    glGenBuffers(1, &ltw_matrices_gl_name);
    glBindBuffer(GL_UNIFORM_BUFFER, ltw_matrices_gl_name);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ltw_matrices), ltw_matrices, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ltw_matrices_gl_name);

    // RateLimitedConsoleLogger tick_logger(1);
    ConsoleLogger tick_logger;
    tick_logger.disable_all_levels();
    std::function<void(double)> tick = [&](double dt) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        tick_logger.debug("1");

        shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                 ShaderUniformVariable::CAMERA_TO_CLIP,
                                 fps_camera.get_projection_matrix(window_width_px, window_height_px));

        shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_SOLID_COLOR,
                                 ShaderUniformVariable::WORLD_TO_CAMERA, fps_camera.get_view_matrix());

        std::vector<unsigned int> ltw_indices(ball.xyz_positions.size(), 0);
        batcher.cwl_v_transformation_ubos_1024_with_solid_color_shader_batcher.queue_draw(
            0, ball.indices, ball.xyz_positions, ltw_indices);

        ltw_matrices[0] = ball_transform.get_transform_matrix();

        batcher.cwl_v_transformation_ubos_1024_with_solid_color_shader_batcher.draw_everything();

        tick_logger.debug("2");

        if (input_state.is_just_pressed(EKey::ESCAPE)) {
            menu_enabled = !menu_enabled;
            if (menu_enabled) {
                fps_camera.freeze_camera();
                window.enable_cursor();
            } else {
                fps_camera.unfreeze_camera();
                window.disable_cursor();
            }
        }

        tick_logger.debug("3");

        if (menu_enabled) {
            tick_logger.debug("3.1");
            input_graphics_sound_menu.process_and_queue_render_menu(window, input_state, ui_render_suite);
        } else {
            tick_logger.debug("3.2");

            fps_camera.process_input(
                // NOTE: guarenteed that value exists and its a valid key due to initialization phase
                input_state.is_pressed(
                    input_state.key_str_to_key_enum.at(configuration.get_value("input", "slow_move").value())),
                input_state.is_pressed(
                    input_state.key_str_to_key_enum.at(configuration.get_value("input", "fast_move").value())),
                input_state.is_pressed(
                    input_state.key_str_to_key_enum.at(configuration.get_value("input", "forward").value())),
                input_state.is_pressed(
                    input_state.key_str_to_key_enum.at(configuration.get_value("input", "left").value())),
                input_state.is_pressed(
                    input_state.key_str_to_key_enum.at(configuration.get_value("input", "back").value())),
                input_state.is_pressed(
                    input_state.key_str_to_key_enum.at(configuration.get_value("input", "right").value())),
                input_state.is_pressed(
                    input_state.key_str_to_key_enum.at(configuration.get_value("input", "up").value())),
                input_state.is_pressed(
                    input_state.key_str_to_key_enum.at(configuration.get_value("input", "down").value())),
                dt);
        }

        tick_logger.debug("4");

        batcher.absolute_position_with_colored_vertex_shader_batcher.draw_everything();

        sound_system.play_all_sounds();

        glBindBuffer(GL_UNIFORM_BUFFER, ltw_matrices_gl_name);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ltw_matrices), ltw_matrices);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        glfwSwapBuffers(window.glfw_window);
        glfwPollEvents();

        tick_logger.debug("5");

        // tick_logger.tick();

        TemporalBinarySignal::process_all();
    };

    std::function<bool()> termination = [&]() { return glfwWindowShouldClose(window.glfw_window); };

    FixedFrequencyLoop ffl;
    ffl.start(120, tick, termination);

    return 0;
}
