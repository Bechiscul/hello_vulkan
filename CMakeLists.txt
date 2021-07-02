cmake_minimum_required(VERSION 3.20)

project(HelloVulkan)

add_executable(App main.cpp)
target_compile_features(App PUBLIC cxx_std_20)

find_package(Vulkan REQUIRED)

target_link_libraries(App PRIVATE Vulkan::Vulkan)