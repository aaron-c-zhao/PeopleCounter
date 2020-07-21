cmake_minimum_required(VERSION 3.15)

project( PeopleCounter)

enable_language(CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(JSON STATIC
	${CMAKE_SOURCE_DIR}/util/json.c )

add_definitions( 
	-DSENSOR_IMAGE_WIDTH=${SENSOR_IMAGE_WIDTH} 
	-DSENSOR_IMAGE_HEIGHT=${SENSOR_IMAGE_HEIGHT}
	-DCT_MAX_DISAPPEARED=${CT_MAX_DISAPPEARED}
	-DCT_MAX_DISTANCE=${CT_MAX_DISTANCE}
	-DRECTS_MAX_SIZE=${RECTS_MAX_SIZE}
	-DTRACKABLE_OBJECT_MAX_SIZE=${TRACKABLE_OBJECT_MAX_SIZE}
	)

message ( STATUS  
	"\\033\[0;31m---------------------------------------------------------------------------------\n"
	${SENSOR_IMAGE_WIDTH} )
configure_file( people_counter.h.in ${CMAKE_SOURCE_DIR}/src/inc/people_counter.h )

set(SOURCE_FILES 
	${CMAKE_SOURCE_DIR}/util/testing_harness.cpp
	${CMAKE_SOURCE_DIR}/src/people_counter.c
	)

find_package ( OpenCV REQUIRED)

include_directories( 
	${OpenCV_INCLUDE_DIRS} 
	${CMAKE_SOURCE_DIR}/util/inc
	${CMAKE_SOURCE_DIR}/src/inc
	)

add_executable ( PeopleCounter ${SOURCE_FILES} )

target_compile_options( PeopleCounter PRIVATE

	-Wall

	$<$<CONFIG:Debug>:-O0>
	$<$<CONFIG:Debug>:-g3>
	$<$<CONFIG:Debug>:-DDBUG>
	)


target_link_libraries( PeopleCounter 
	${OpenCV_LIBS} 
	JSON)

