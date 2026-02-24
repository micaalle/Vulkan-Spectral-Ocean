cmake_minimum_required(VERSION 3.20)
project(VulkanOcean LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

include(FetchContent)

# GLFW (Vulkan surface + input)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

add_executable(VulkanOcean
  src/main.cpp
  src/vk_context.cpp
  src/vk_helpers.cpp
  src/hdr_loader.cpp
  src/obj_loader.cpp
)


target_include_directories(VulkanOcean PRIVATE
  ${Vulkan_INCLUDE_DIRS}
  ${glm_SOURCE_DIR}
  src
)

target_link_libraries(VulkanOcean PRIVATE
  Vulkan::Vulkan
  glfw
)

target_compile_definitions(VulkanOcean PRIVATE
  GLFW_INCLUDE_VULKAN
)

if (MSVC)
  target_compile_options(VulkanOcean PRIVATE /W4 /permissive-)
else()
  target_compile_options(VulkanOcean PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32")
if (NOT GLSLC)
  message(FATAL_ERROR "glslc not found.")
endif()

set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_BINARY_DIR}/shaders_spv)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADERS
  spectrum.comp
  build_tiles.comp
  fft_combine.comp
  ifft_rows.comp
  ifft_cols.comp
  foam.comp
  spray_update.comp
  spray_spawn.comp
  water.vert
  water.frag
  boat.vert
  boat.frag
  skybox.vert
  skybox.frag
  skybox_scene.frag
  fullscreen.vert
  taa.frag
  tonemap.frag
  spray.vert
  spray.frag
  cube_capture.vert
  equirect_to_cubemap.frag
)

set(SPVS "")
foreach(SH ${SHADERS})
  set(SRC ${SHADER_SRC_DIR}/${SH})
  set(OUT ${SHADER_OUT_DIR}/${SH}.spv)
  list(APPEND SPVS ${OUT})

  add_custom_command(
    OUTPUT ${OUT}
    COMMAND ${GLSLC} --target-env=vulkan1.2 -O ${SRC} -o ${OUT}
    DEPENDS ${SRC}
    COMMENT "Compiling shader ${SH}"
    VERBATIM
  )
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPVS})
add_dependencies(VulkanOcean Shaders)

add_custom_command(TARGET VulkanOcean POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:VulkanOcean>/shaders_spv
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_OUT_DIR} $<TARGET_FILE_DIR:VulkanOcean>/shaders_spv
)

add_custom_command(TARGET VulkanOcean POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:VulkanOcean>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:VulkanOcean>/assets
)
