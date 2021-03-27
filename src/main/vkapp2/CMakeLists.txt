add_library(graphics STATIC
	cmdpool.cpp swapchain.cpp vk_utils.cpp
	mem.cpp model.cpp pipeline.cpp renderpass.cpp
	texture.cpp application_run.cpp
	settings/options.cpp settings/scene.cpp)

add_executable(vkapp2 application.cpp)
add_dependencies(vkapp2 shaders-glslc)
target_link_libraries(vkapp2
	graphics util
	glfw vulkan config++)
