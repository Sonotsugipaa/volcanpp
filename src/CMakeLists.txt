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
