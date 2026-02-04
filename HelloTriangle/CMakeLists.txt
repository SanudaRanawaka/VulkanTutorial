# CMakeList.txt : CMake project for HelloTriangle, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (HelloTriangle "HelloTriangle.cpp" "HelloTriangle.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HelloTriangle PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
target_link_libraries(HelloTriangle
    PRIVATE
        Vulkan::Vulkan
        glfw
)
#add_slang_shader_target(compile_shaders SOURCES ${SHADER_SLANG_SOURCES})
#add_dependencies(HelloTriangle compile_shaders)