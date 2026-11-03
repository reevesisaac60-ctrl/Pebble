#include "pebble/package.hpp"

namespace pebble {

const std::vector<Package>& builtinPackages() {
    static const std::vector<Package> packages = {
        Package{
            "math",
            {
                PackageFunction{
                    "randint",
                    {"Int", "Int"},
                    "Int",
                    "pebble_math_randint"
                },
                PackageFunction{
                    "abs",
                    {"Int"},
                    "Int",
                    "pebble_math_abs"
                },
                PackageFunction{
                    "min",
                    {"Int", "Int"},
                    "Int",
                    "pebble_math_min"
                },
                PackageFunction{
                    "max",
                    {"Int", "Int"},
                    "Int",
                    "pebble_math_max"
                },
                PackageFunction{
                    "randomseed",
                    {"Int"},
                    "Void",
                    "pebble_math_randomseed"
                },
                PackageFunction{
                    "random",
                    {},
                    "Float",
                    "pebble_math_random"
                },
                PackageFunction{
                    "sqrt",
                    {"Float"},
                    "Float",
                    "pebble_math_sqrt"
                },
                PackageFunction{
                    "floor",
                    {"Float"},
                    "Float",
                    "pebble_math_floor"
                },
                PackageFunction{
                    "ceil",
                    {"Float"},
                    "Float",
                    "pebble_math_ceil"
                },
                PackageFunction{
                    "round",
                    {"Float"},
                    "Float",
                    "pebble_math_round"
                },
                PackageFunction{
                    "pi",
                    {},
                    "Float",
                    "pebble_math_pi"
                }
            }
        },
        Package{
            "string",
            {
                PackageFunction{
                    "length",
                    {"String"},
                    "Int",
                    "pebble_string_length"
                },
                PackageFunction{
                    "substring",
                    {"String", "Int", "Int"},
                    "String",
                    "pebble_string_substring"
                },
                PackageFunction{
                    "left",
                    {"String", "Int"},
                    "String",
                    "pebble_string_left"
                },
                PackageFunction{
                    "right",
                    {"String", "Int"},
                    "String",
                    "pebble_string_right"
                },
                PackageFunction{
                    "upper",
                    {"String"},
                    "String",
                    "pebble_string_upper"
                },
                PackageFunction{
                    "lower",
                    {"String"},
                    "String",
                    "pebble_string_lower"
                },
                PackageFunction{
                    "trim",
                    {"String"},
                    "String",
                    "pebble_string_trim"
                },
                PackageFunction{
                    "contains",
                    {"String", "String"},
                    "Bool",
                    "pebble_string_contains"
                },
                PackageFunction{
                    "startswith",
                    {"String", "String"},
                    "Bool",
                    "pebble_string_startswith"
                },
                PackageFunction{
                    "endswith",
                    {"String", "String"},
                    "Bool",
                    "pebble_string_endswith"
                },
                PackageFunction{
                    "indexof",
                    {"String", "String"},
                    "Int",
                    "pebble_string_indexof"
                },
                PackageFunction{
                    "replace",
                    {"String", "String", "String"},
                    "String",
                    "pebble_string_replace"
                },
                PackageFunction{
                    "repeat",
                    {"String", "Int"},
                    "String",
                    "pebble_string_repeat"
                },
                PackageFunction{
                    "toint",
                    {"String"},
                    "Int",
                    "pebble_string_toint"
                },
                PackageFunction{
                    "tofloat",
                    {"String"},
                    "Float",
                    "pebble_string_tofloat"
                }
            }
        }

        ,
        Package{
            "time",
            {
                PackageFunction{
                    "now",
                    {},
                    "Float",
                    "pebble_time_now"
                },
                PackageFunction{
                    "ticks",
                    {},
                    "Int",
                    "pebble_time_ticks"
                },
                PackageFunction{
                    "delta",
                    {},
                    "Float",
                    "pebble_time_delta"
                },
                PackageFunction{
                    "sleep",
                    {"Float"},
                    "Void",
                    "pebble_time_sleep"
                },
                PackageFunction{
                    "unix",
                    {},
                    "Int",
                    "pebble_time_unix"
                }
            }
        }

        ,
        Package{
            "filesystem",
            {
                PackageFunction{
                    "read",
                    {"String"},
                    "String",
                    "pebble_filesystem_read"
                },
                PackageFunction{
                    "write",
                    {"String", "String"},
                    "Void",
                    "pebble_filesystem_write"
                },
                PackageFunction{
                    "exists",
                    {"String"},
                    "Bool",
                    "pebble_filesystem_exists"
                },
                PackageFunction{
                    "list",
                    {"String"},
                    "String",
                    "pebble_filesystem_list"
                },
                PackageFunction{
                    "mkdir",
                    {"String"},
                    "Void",
                    "pebble_filesystem_mkdir"
                }
            }
        }

        ,
        Package{
            "Log",
            {
                PackageFunction{"Print", {"String"}, "Void", "pebble_log_print"},
                PackageFunction{"Warning", {"String"}, "Void", "pebble_log_warning"},
                PackageFunction{"Error", {"String"}, "Void", "pebble_log_error"}
            }
        },
        Package{
            "Math",
            {
                PackageFunction{"Abs", {"Int"}, "Int", "pebble_math_abs"},
                PackageFunction{"Min", {"Int", "Int"}, "Int", "pebble_math_min"},
                PackageFunction{"Max", {"Int", "Int"}, "Int", "pebble_math_max"},
                PackageFunction{"Sqrt", {"Float"}, "Float", "pebble_math_sqrt"},
                PackageFunction{"Floor", {"Float"}, "Float", "pebble_math_floor"},
                PackageFunction{"Ceil", {"Float"}, "Float", "pebble_math_ceil"},
                PackageFunction{"Round", {"Float"}, "Float", "pebble_math_round"},
                PackageFunction{"Pi", {}, "Float", "pebble_math_pi"},
                PackageFunction{"Radians", {"Float"}, "Float", "pebble_math_radians"},
                PackageFunction{"Sin", {"Float"}, "Float", "pebble_math_sin"},
                PackageFunction{"Cos", {"Float"}, "Float", "pebble_math_cos"},
                PackageFunction{"Tan", {"Float"}, "Float", "pebble_math_tan"}
            }
        }

        ,
        Package{
            "Window",
            {
                PackageFunction{"Create", {"String", "Int", "Int"}, "Void", "pebble_window_create"},
                PackageFunction{"ShouldClose", {}, "Bool", "pebble_window_should_close"},
                PackageFunction{"PollEvents", {}, "Void", "pebble_window_poll_events"},
                PackageFunction{"SwapBuffers", {}, "Void", "pebble_window_swap_buffers"},
                PackageFunction{"Close", {}, "Void", "pebble_window_close"}
            }
        }

        ,
        Package{
            "Input",
            {
                PackageFunction{"IsKeyDown", {"String"}, "Bool", "pebble_input_is_key_down"},
                PackageFunction{"IsMouseButtonDown", {"Int"}, "Bool", "pebble_input_is_mouse_button_down"},
                PackageFunction{"MouseX", {}, "Float", "pebble_input_mouse_x"},
                PackageFunction{"MouseY", {}, "Float", "pebble_input_mouse_y"}
            }
        }

        ,
        Package{
            "OpenGL",
            {
                PackageFunction{"Initialize", {}, "Bool", "pebble_gl_initialize"},
                PackageFunction{"Shutdown", {}, "Void", "pebble_gl_shutdown"},
                PackageFunction{"Viewport", {"Int", "Int"}, "Void", "pebble_gl_viewport"},
                PackageFunction{"ClearColor", {"Float", "Float", "Float", "Float"}, "Void", "pebble_gl_clear_color"},
                PackageFunction{"Clear", {}, "Void", "pebble_gl_clear"},
                PackageFunction{"EnableDepthTest", {}, "Void", "pebble_gl_enable_depth_test"},
                PackageFunction{"CreateProgram", {"String", "String"}, "Int", "pebble_gl_create_program"},
                PackageFunction{"UseProgram", {"Int"}, "Void", "pebble_gl_use_program"},
                PackageFunction{"DeleteProgram", {"Int"}, "Void", "pebble_gl_delete_program"},
                PackageFunction{"UniformLocation", {"Int", "String"}, "Int", "pebble_gl_uniform_location"},
                PackageFunction{"UniformInt", {"Int", "Int"}, "Void", "pebble_gl_uniform_int"},
                PackageFunction{"UniformFloat", {"Int", "Float"}, "Void", "pebble_gl_uniform_float"},
                PackageFunction{"UniformVec2", {"Int", "Float", "Float"}, "Void", "pebble_gl_uniform_vec2"},
                PackageFunction{"UniformVec3", {"Int", "Float", "Float", "Float"}, "Void", "pebble_gl_uniform_vec3"},
                PackageFunction{"UniformVec4", {"Int", "Float", "Float", "Float", "Float"}, "Void", "pebble_gl_uniform_vec4"},
                PackageFunction{"UniformMat4", {"Int", "Float", "Float", "Float", "Float", "Float", "Float", "Float", "Float", "Float", "Float", "Float", "Float", "Float", "Float", "Float", "Float"}, "Void", "pebble_gl_uniform_mat4"},
                PackageFunction{"CreateVertexArray", {}, "Int", "pebble_gl_create_vertex_array"},
                PackageFunction{"BindVertexArray", {"Int"}, "Void", "pebble_gl_bind_vertex_array"},
                PackageFunction{"DeleteVertexArray", {"Int"}, "Void", "pebble_gl_delete_vertex_array"},
                PackageFunction{"CreateBuffer", {}, "Int", "pebble_gl_create_buffer"},
                PackageFunction{"BindArrayBuffer", {"Int"}, "Void", "pebble_gl_bind_array_buffer"},
                PackageFunction{"BindElementBuffer", {"Int"}, "Void", "pebble_gl_bind_element_buffer"},
                PackageFunction{"UploadArrayBuffer", {"String"}, "Void", "pebble_gl_upload_array_buffer"},
                PackageFunction{"UploadElementBuffer", {"String"}, "Void", "pebble_gl_upload_element_buffer"},
                PackageFunction{"VertexAttribute", {"Int", "Int", "Int", "Int"}, "Void", "pebble_gl_vertex_attribute"},
                PackageFunction{"DrawElements", {"Int"}, "Void", "pebble_gl_draw_elements"},
                PackageFunction{"DrawArrays", {"Int"}, "Void", "pebble_gl_draw_arrays"},
                PackageFunction{"DeleteBuffer", {"Int"}, "Void", "pebble_gl_delete_buffer"},
                PackageFunction{"CreateTexture", {}, "Int", "pebble_gl_create_texture"},
                PackageFunction{"BindTexture", {"Int", "Int"}, "Void", "pebble_gl_bind_texture"},
                PackageFunction{"UploadTextureRgba", {"Int", "Int", "String"}, "Void", "pebble_gl_upload_texture_rgba"},
                PackageFunction{"SetTextureSampling", {"String", "String"}, "Void", "pebble_gl_set_texture_sampling"},
                PackageFunction{"DeleteTexture", {"Int"}, "Void", "pebble_gl_delete_texture"}
            }
        }

    };

    return packages;
}

const Package* findBuiltinPackage(const std::string& name) {
    for (const Package& package : builtinPackages()) {
        if (package.name == name) {
            return &package;
        }
    }

    return nullptr;
}

const PackageFunction* findPackageFunction(
    const Package& package,
    const std::string& name
) {
    for (const PackageFunction& function : package.functions) {
        if (function.name == name) {
            return &function;
        }
    }

    return nullptr;
}

}
