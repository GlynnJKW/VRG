cmake_minimum_required(VERSION 3.20)

project(VulkanRenderGraph)

# Dependencies

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)

file(DOWNLOAD https://raw.githubusercontent.com/malte-v/VulkanMemoryAllocator-Hpp/master/vk_mem_alloc.h         ${CMAKE_CURRENT_LIST_DIR}/src/3rdParty/vk_mem_alloc.h)
file(DOWNLOAD https://raw.githubusercontent.com/malte-v/VulkanMemoryAllocator-Hpp/master/vk_mem_alloc.hpp       ${CMAKE_CURRENT_LIST_DIR}/src/3rdParty/vk_mem_alloc.h)

add_subdirectory(src)