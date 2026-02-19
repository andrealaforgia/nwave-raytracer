add_library(nwave_core
    core/aabb.cpp
)
target_include_directories(nwave_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(NWAVE_HAS_METAL)
    target_compile_definitions(nwave_core PUBLIC NWAVE_HAS_METAL=1)
    target_link_libraries(nwave_core PUBLIC "${METAL_FRAMEWORK}" "${FOUNDATION_FRAMEWORK}")
endif()

add_library(nwave_domain
    domain/shapes/sphere.cpp
    domain/shapes/plane.cpp
    domain/shapes/triangle.cpp
    domain/shapes/triangle_mesh.cpp
    domain/shapes/box.cpp
    domain/shapes/cylinder.cpp
    domain/shapes/transformed_shape.cpp
    domain/shapes/deformable_mesh.cpp
    domain/shapes/letter_shapes.cpp
    domain/materials/lambertian.cpp
    domain/materials/metal.cpp
    domain/materials/dielectric.cpp
    domain/materials/emissive.cpp
    domain/lights/point_light.cpp
    domain/lights/directional_light.cpp
    domain/camera.cpp
    domain/scene.cpp
)
target_include_directories(nwave_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nwave_domain PUBLIC nwave_core)

add_library(nwave_application
    application/renderer.cpp
    application/animation_renderer.cpp
    application/cpu_render_backend.cpp
)
target_include_directories(nwave_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nwave_application PUBLIC nwave_domain)

add_library(nwave_infrastructure
    infrastructure/ppm_writer.cpp
    infrastructure/yaml_scene_loader.cpp
    infrastructure/cli_dispatcher.cpp
    infrastructure/validator.cpp
    infrastructure/jolt_physics_simulator.cpp
    infrastructure/progress_reporter.cpp
    infrastructure/gpu/scene_flattener.cpp
    infrastructure/gpu/bvh_flattener.cpp
)
target_include_directories(nwave_infrastructure PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nwave_infrastructure PUBLIC nwave_application yaml-cpp::yaml-cpp Jolt)

if(NWAVE_HAS_METAL)
    target_sources(nwave_infrastructure PRIVATE
        infrastructure/metal/metal_device.mm
        infrastructure/metal/metal_buffer_manager.mm
        infrastructure/metal/metal_render_backend.mm
    )

    # --- Metal shader compilation: .metal -> .air -> .metallib ---
    # Detect Metal shader compiler (requires full Xcode, not just Command Line Tools)
    execute_process(
        COMMAND xcrun --find metal
        OUTPUT_VARIABLE METAL_COMPILER
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE METAL_COMPILER_FOUND
    )

    if(METAL_COMPILER_FOUND EQUAL 0)
        message(STATUS "Metal shader compiler found: ${METAL_COMPILER}")
        set(NWAVE_HAS_METAL_COMPILER ON)
    else()
        message(WARNING "Metal shader compiler not found (requires Xcode). Shader compilation disabled.")
        set(NWAVE_HAS_METAL_COMPILER OFF)
    endif()

    if(NWAVE_HAS_METAL_COMPILER)
        set(METAL_GRADIENT_SOURCE
            ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/metal/shaders/gradient.metal)
        set(METAL_RAY_TRACE_SOURCE
            ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/metal/shaders/ray_trace.metal)
        set(METAL_GRADIENT_AIR
            ${CMAKE_CURRENT_BINARY_DIR}/gradient.air)
        set(METAL_RAY_TRACE_AIR
            ${CMAKE_CURRENT_BINARY_DIR}/ray_trace.air)
        set(METAL_LIB_OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/nwave_shaders.metallib)

        add_custom_command(
            OUTPUT ${METAL_GRADIENT_AIR}
            COMMAND xcrun -sdk macosx metal -c ${METAL_GRADIENT_SOURCE} -o ${METAL_GRADIENT_AIR}
            DEPENDS ${METAL_GRADIENT_SOURCE}
            COMMENT "Compiling gradient.metal -> gradient.air"
        )

        add_custom_command(
            OUTPUT ${METAL_RAY_TRACE_AIR}
            COMMAND xcrun -sdk macosx metal -c ${METAL_RAY_TRACE_SOURCE} -o ${METAL_RAY_TRACE_AIR}
            DEPENDS ${METAL_RAY_TRACE_SOURCE}
            COMMENT "Compiling ray_trace.metal -> ray_trace.air"
        )

        add_custom_command(
            OUTPUT ${METAL_LIB_OUTPUT}
            COMMAND xcrun -sdk macosx metallib ${METAL_GRADIENT_AIR} ${METAL_RAY_TRACE_AIR} -o ${METAL_LIB_OUTPUT}
            DEPENDS ${METAL_GRADIENT_AIR} ${METAL_RAY_TRACE_AIR}
            COMMENT "Linking gradient.air + ray_trace.air -> nwave_shaders.metallib"
        )

        add_custom_target(nwave_shaders ALL DEPENDS ${METAL_LIB_OUTPUT})

        # Copy metallib next to all binary targets that need it
        add_custom_command(TARGET nwave_shaders POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${METAL_LIB_OUTPUT}
                ${CMAKE_BINARY_DIR}/tests/nwave_shaders.metallib
            COMMENT "Copying metallib to test binary directory"
        )
        add_custom_command(TARGET nwave_shaders POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${METAL_LIB_OUTPUT}
                ${CMAKE_BINARY_DIR}/nwave_shaders.metallib
            COMMENT "Copying metallib to main binary directory"
        )
    endif()
endif()

add_executable(nwave main.cpp)
target_link_libraries(nwave PRIVATE nwave_infrastructure)
