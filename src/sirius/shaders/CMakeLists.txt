set(SHADER_SOURCES
        simple_shader.vert
        simple_shader.frag
        gradient.comp
        gradient_color.comp
        sky.comp
        colored_triangle.frag
        colored_triangle.vert
        colored_triangle_mesh.vert
)

set(SHADER_SPV)

foreach (SHADER ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPV ${CMAKE_CURRENT_SOURCE_DIR}/${FILE_NAME}.spv)
    message("$ENV{Vulkan_GLSLC_EXECUTABLE}" ${SHADER} -o ${SPV})

    add_custom_command(
            OUTPUT ${SPV}
            COMMAND glslc.exe ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER} -o ${SPV}
            DEPENDS ${SHADER}
            COMMENT "Compiling ${FILE_NAME} to SPIRV-V ${SPV}"
            VERBATIM
    )

    list(APPEND SHADER_SPV ${SPV})
endforeach ()

add_custom_target(shaders ALL DEPENDS ${SHADER_SPV})

