set(shader_destdir ${CMAKE_CURRENT_BINARY_DIR})

macro(add_shader src dest stage)
	list(APPEND SHADER_TARGETS "${shader_destdir}/${dest}")
	add_custom_command(
		OUTPUT "${shader_destdir}/${dest}"
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src}
		COMMAND glslc
			-fshader-stage=${stage}
			${CMAKE_CURRENT_SOURCE_DIR}/${src}
			-o ${shader_destdir}/${dest})
endmacro()

add_shader( vertex.main.glsl       vertex.main.spv       vertex   )
add_shader( vertex.outline.glsl    vertex.outline.spv    vertex   )
add_shader( fragment.main.glsl     fragment.main.spv     fragment )
add_shader( fragment.outline.glsl  fragment.outline.spv  fragment )

add_custom_command(OUTPUT ${shader_destdir} COMMAND mkdir -p ${shader_destdir})
add_custom_target(shaders-glslc ALL DEPENDS ${shader_destdir} ${SHADER_TARGETS})
