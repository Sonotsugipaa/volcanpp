set(asset_destdir ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB assets "*.png" "*.obj" "*.cfg")
foreach(asset ${assets})
	get_filename_component(asset_name ${asset} NAME)
	file(CREATE_LINK
		"${asset}"
		"${asset_destdir}/${asset_name}" COPY_ON_ERROR)
endforeach()
