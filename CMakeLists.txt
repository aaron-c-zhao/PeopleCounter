cmake_minimum_required(VERSION 3.15)

project( PeopleCounter)

enable_language(CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
  string(ASCII 27 Esc)
  set(ColourReset "${Esc}[m")
  set(ColourBold  "${Esc}[1m")
  set(Red         "${Esc}[31m")
  set(Green       "${Esc}[32m")
  set(Yellow      "${Esc}[33m")
  set(Blue        "${Esc}[34m")
  set(Magenta     "${Esc}[35m")
  set(Cyan        "${Esc}[36m")
  set(White       "${Esc}[37m")
  set(BoldRed     "${Esc}[1;31m")
  set(BoldGreen   "${Esc}[1;32m")
  set(BoldYellow  "${Esc}[1;33m")
  set(BoldBlue    "${Esc}[1;34m")
  set(BoldMagenta "${Esc}[1;35m")
  set(BoldCyan    "${Esc}[1;36m")
  set(BoldWhite   "${Esc}[1;37m")
endif()

add_library(JSON STATIC
	${CMAKE_SOURCE_DIR}/util/json.c )

add_definitions( 
	-DSENSOR_IMAGE_WIDTH=${SENSOR_IMAGE_WIDTH} 
	-DSENSOR_IMAGE_HEIGHT=${SENSOR_IMAGE_HEIGHT}
	-DCT_MAX_DISAPPEARED=${CT_MAX_DISAPPEARED}
	-DCT_MAX_DISTANCE=${CT_MAX_DISTANCE}
	-DRECTS_MAX_SIZE=${RECTS_MAX_SIZE}
	-DTRACKABLE_OBJECT_MAX_SIZE=${TRACKABLE_OBJECT_MAX_SIZE}
	-DFRAME_RATE=${FRAME_RATE}
	-DLOG_KERNEL_SIZE=${LOG_KERNEL_SIZE}
	)

message ( 
	"${Red}-----------------------------------------------------------------------------------------${ColorRest}\n"
	"${Green}Image width:${Red}${SENSOR_IMAGE_WIDTH}${ColourReset}\n" 
	"${Green}Image height:${Red}${SENSOR_IMAGE_HEIGHT}${ColourReset}\n"
	"${Green}CT max disappera:${Red}${CT_MAX_DISAPPEARED}${ColourReset}\n"
	"${Green}CT max distance:${Red}${CT_MAX_DISTANCE}${ColourReset}\n"
	"${Green}Rects max amount:${Red}${RECTS_MAX_SIZE}${ColourReset}\n"
	"${Green}Trackable object max amount:${Red}${TRACKABLE_OBJECT_MAX_SIZE}${ColourReset}\n"
	"${Green}Log kernel size:${Red}${LOG_KERNEL_SIZE}${ColorReset}\n"
	"${Green}Frame rate:${Red}${FRAME_RATE}${ColouerReset}\n"
	"${Red}-----------------------------------------------------------------------------------------${ColourReset}\n"
	)
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

