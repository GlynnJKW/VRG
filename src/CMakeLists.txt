cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED TRUE)

file(GLOB_RECURSE VRG_CPP "**.cpp")
add_executable(VulkanRenderGraph "${VRG_CPP}" )

target_include_directories(VulkanRenderGraph PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty"
)

target_link_libraries(VulkanRenderGraph PUBLIC Vulkan shaderc_combined)

if(WIN32)
    target_compile_definitions(VulkanRenderGraph PUBLIC VK_USE_PLATFORM_WIN32_KHR WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

