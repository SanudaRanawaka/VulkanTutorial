# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("HelloTriangle")


find_package(Vulkan REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

#find_library(glfw NAMES glfw3 REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

 # "C:\vcpkg\installed\x64-windows\share\glfw3\glfw3Config.cmake"
 # "C:\vcpkg\installed\x64-windows\lib\glfw3dll.lib"
 # "C:\vcpkg\installed\x64-windows\debug\lib\glfw3dll.lib"

 # "C:\vcpkg\packages\glfw3_x64-windows\share\glfw3\glfw3Config.cmake"
 # "C:\vcpkg\packages\glfw3_x64-windows\lib\glfw3dll.lib"
 # "C:\vcpkg\packages\glfw3_x64-windows\debug\lib\glfw3dll.lib"
 
 #function (add_slang_shader_target TARGET)
  #cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  #set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  #set (ENTRY_POINTS -entry vertMain -entry fragMain)
  #add_custom_command (
  #        OUTPUT ${SHADERS_DIR}
  #        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  #)
  #add_custom_command (
  #        OUTPUT  ${SHADERS_DIR}/slang.spv
  #        COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang.spv
  #        WORKING_DIRECTORY ${SHADERS_DIR}
  #        DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
  #        COMMENT "Compiling Slang Shaders"
  #        VERBATIM
  #)
  #add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
#endfunction()

# Include sub-projects.
add_subdirectory ("HelloTriangle")
