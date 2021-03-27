# Declare the project

	cmake_minimum_required(VERSION 3.12)
	project(vkapp2)


# Set the project's structure

	set(PROJECT_SOURCE_DIR .)
	set(PROJECT_BINARY_DIR .)

	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/ar)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

	include_directories(../include)
	link_directories(../lib)
	link_directories(../bin)


# Set warning levels

	add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)


# Define the C and C++ standards to use

	set(CMAKE_C_STANDARD 11)
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_C_STANDARD_REQUIRED False)
	set(CMAKE_CXX_STANDARD_REQUIRED True)


# Add the main and test subdirectories

	add_subdirectory(${PROJECT_SOURCE_DIR}/main)
	add_subdirectory(${PROJECT_SOURCE_DIR}/test)
