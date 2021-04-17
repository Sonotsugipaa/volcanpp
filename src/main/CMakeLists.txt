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


# Add locally used includes (should not be shipped)
include_directories(.)

# Add all component source directories
add_subdirectory(vkapp2)
add_subdirectory(shaders)
add_subdirectory(assets)


# Process top-level targets

	function(copy_file file)
		configure_file(
			${CMAKE_CURRENT_SOURCE_DIR}/${file}
			${CMAKE_CURRENT_BINARY_DIR}/${file}
			COPYONLY)
	endfunction(copy_file)

	function(copy_file_opt file)
		if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
			copy_file("${file}")
		else()
			message("File \"${file}\" does not exist, not copying")
		endif()
	endfunction(copy_file_opt)

	copy_file(launch.sh)
	copy_file_opt(params.cfg)

	# The "util" directory is not a CMake subfolder,
	# since it's meant to be shared with all the ideally
	# self-contained modules
	add_library(util STATIC
		util/util.cpp)
