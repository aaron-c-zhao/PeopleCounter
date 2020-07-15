cmake_minimum_required(VERSION 3.15)

project( PeopleCounter)

enable_language(CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(JSON STATIC
	${CMAKE_SOURCE_DIR}/util/json.c )

set(SOURCE_FILES 
	${CMAKE_SOURCE_DIR}/util/testing_harness.cpp
	)

find_package ( OpenCV REQUIRED)

include_directories( 
	${OpenCV_INCLUDE_DIRS} 
	${CMAKE_SOURCE_DIR}/util/inc
	)
add_executable ( PeopleCounter ${SOURCE_FILES} )

target_link_libraries( PeopleCounter 
	${OpenCV_LIBS} 
	JSON)


