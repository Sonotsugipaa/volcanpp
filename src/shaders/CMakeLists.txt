# MIT License
#
# Copyright (c) 2021 Parola Marco
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


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
