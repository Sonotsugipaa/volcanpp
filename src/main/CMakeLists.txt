# Add locally used includes (should not be shupped)
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
