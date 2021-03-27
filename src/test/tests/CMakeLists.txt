macro(add_test name dependency)
	add_executable(unit-test-${name} ${name}.cpp)
	target_link_libraries(unit-test-${name}
		test-tools ${dependency})
endmacro(add_test)
