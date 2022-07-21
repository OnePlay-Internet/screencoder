cmake_minimum_required(VERSION 3.0)

project(SunshineSDK VERSION 0.1.0 DESCRIPTION "Sunshine is a Gamestream host for Moonlight.")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(SUNSHINE_SOURCE_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")






enable_language(RC)
set(CMAKE_RC_COMPILER windres)
file(
	DOWNLOAD "https://github.com/TheElixZammuto/sunshine-prebuilt/releases/download/1.0.0/pre-compiled.zip" "${CMAKE_CURRENT_BINARY_DIR}/pre-compiled.zip"
	TIMEOUT 60
	EXPECTED_HASH SHA256=5d59986bd7f619eaaf82b2dd56b5127b747c9cbe8db61e3b898ff6b485298ed6)
file(ARCHIVE_EXTRACT
	INPUT "${CMAKE_CURRENT_BINARY_DIR}/pre-compiled.zip"
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/pre-compiled)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

if(NOT DEFINED SUNSHINE_PREPARED_BINARIES)
	set(SUNSHINE_PREPARED_BINARIES "${CMAKE_CURRENT_BINARY_DIR}/pre-compiled/windows")
endif()


set(PLATFORM_INCLUDE_DIR
	${SUNSHINE_PREPARED_BINARIES}/platform
	${SUNSHINE_PREPARED_BINARIES}/platform/windows
    )

set(FFMPEG_INCLUDE_DIRS
	${SUNSHINE_PREPARED_BINARIES}/include)

set(FFMPEG_LIBRARIES
	${SUNSHINE_PREPARED_BINARIES}/lib/libavformat.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libavcodec.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libavdevice.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libavfilter.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libavutil.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libpostproc.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libswresample.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libswscale.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libx264.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libx265.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libhdr10plus.a
	z lzma bcrypt libiconv.a)

list(PREPEND PLATFORM_LIBRARIES
	libstdc++.a
	libwinpthread.a
	libssp.a
	ksuser
	wsock32
	ws2_32
	d3d11 dxgi D3DCompiler
	setupapi
	dwmapi
	bz2
	Secur32
	swresample
	Bcrypt
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB UTIL_SOURCE_LIST 
	util/*.cpp
	util/queue/*.cpp
	util/object/*.cpp
	util/macro/*.cpp
	util/event/*.cpp
	util/log/*.cpp
	util/datatype/*.cpp
	util/avcodec/*.cpp
	util/array/*.cpp
)

file(GLOB ENCODER_SOURCE_LIST 
	encoder/*.cpp
	encoder/d3d11/*.cpp
	encoder/bitstream/*.cpp
)

file(GLOB RTP_SOURCE_LIST 
	rtp/*.cpp
)

file(GLOB CONFIG_SOURCE_LIST 
	config/*.cpp
)

file(GLOB SESSION_SOURCE_LIST 
	session/*.cpp
)

file(GLOB PLATFORM_SOURCE_LIST 
	platform/*.cpp
	platform/windows/*.cpp
)


set(SUNSHINE_TARGET_FILES
	${UTIL_SOURCE_LIST}
	${SESSION_SOURCE_LIST}
	${ENCODER_SOURCE_LIST}
	${RTP_SOURCE_LIST}
	${CONFIG_SOURCE_LIST}
	${PLATFORM_SOURCE_LIST}
)



include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/config
  ${CMAKE_CURRENT_SOURCE_DIR}/audio

  ${CMAKE_CURRENT_SOURCE_DIR}/encoder
  ${CMAKE_CURRENT_SOURCE_DIR}/encoder/bitstream
  ${CMAKE_CURRENT_SOURCE_DIR}/encoder/d3d11

  ${CMAKE_CURRENT_SOURCE_DIR}/input
  ${CMAKE_CURRENT_SOURCE_DIR}/rtp
  ${CMAKE_CURRENT_SOURCE_DIR}/session

  ${CMAKE_CURRENT_SOURCE_DIR}/util
  ${CMAKE_CURRENT_SOURCE_DIR}/util/queue
  ${CMAKE_CURRENT_SOURCE_DIR}/util/object
  ${CMAKE_CURRENT_SOURCE_DIR}/util/macro
  ${CMAKE_CURRENT_SOURCE_DIR}/util/event
  ${CMAKE_CURRENT_SOURCE_DIR}/util/log
  ${CMAKE_CURRENT_SOURCE_DIR}/util/datatype
  ${CMAKE_CURRENT_SOURCE_DIR}/util/avcodec
  ${CMAKE_CURRENT_SOURCE_DIR}/util/array

  ${CMAKE_CURRENT_SOURCE_DIR}/platform
  ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows

  ${FFMPEG_INCLUDE_DIRS}
  ${PLATFORM_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIR}
)
 



list(APPEND SUNSHINE_EXTERNAL_LIBRARIES
    ${CBS_EXTERNAL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${FFMPEG_LIBRARIES}
    ${PLATFORM_LIBRARIES}
    ${Boost_LIBRARIES}
)

add_executable(sunshine 
	main.cpp
	${SUNSHINE_TARGET_FILES}
)


target_link_libraries(sunshine ${SUNSHINE_EXTERNAL_LIBRARIES} ${EXTRA_LIBS})
target_compile_definitions(sunshine PUBLIC ${SUNSHINE_DEFINITIONS})
set_target_properties(sunshine PROPERTIES CXX_STANDARD 17
                            VERSION ${PROJECT_VERSION}
                            SOVERSION ${PROJECT_VERSION_MAJOR}
                            )


