add_library(nwave_core
    core/aabb.cpp
)
target_include_directories(nwave_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(nwave_domain
    domain/shapes/sphere.cpp
    domain/shapes/plane.cpp
    domain/shapes/triangle.cpp
    domain/shapes/triangle_mesh.cpp
    domain/shapes/box.cpp
    domain/shapes/cylinder.cpp
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
)
target_include_directories(nwave_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nwave_application PUBLIC nwave_domain)

add_library(nwave_infrastructure
    infrastructure/ppm_writer.cpp
    infrastructure/yaml_scene_loader.cpp
)
target_include_directories(nwave_infrastructure PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nwave_infrastructure PUBLIC nwave_application yaml-cpp::yaml-cpp)

add_executable(nwave main.cpp)
target_link_libraries(nwave PRIVATE nwave_infrastructure)
