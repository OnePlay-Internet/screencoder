cmake_minimum_required(VERSION 3.0)

set(CMAKE_BUILD_TYPE "Debug")
project(SunshineSDK VERSION 0.1.0 DESCRIPTION "Sunshine is a Gamestream host for Moonlight.")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(SCREENCODER_SOURCE_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")






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

if(NOT DEFINED SCREENCODER_PREPARED_BINARIES)
	set(SCREENCODER_PREPARED_BINARIES "${CMAKE_CURRENT_BINARY_DIR}/pre-compiled/windows")
endif()


set(PLATFORM_INCLUDE_DIR
	${SCREENCODER_PREPARED_BINARIES}/platform
	${SCREENCODER_PREPARED_BINARIES}/platform/windows
    )

set(FFMPEG_INCLUDE_DIRS
	${SCREENCODER_PREPARED_BINARIES}/include)

set(FFMPEG_LIBRARIES
	${SCREENCODER_PREPARED_BINARIES}/lib/libavformat.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libavcodec.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libavdevice.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libavfilter.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libavutil.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libpostproc.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libswresample.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libswscale.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libx264.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libx265.a
	${SCREENCODER_PREPARED_BINARIES}/lib/libhdr10plus.a
	z lzma bcrypt libiconv.a
	)

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
	encode/*.cpp
	encode/encoder/*.cpp
	encode/encoder/d3d11/*.cpp
)

file(GLOB RTP_SOURCE_LIST 
	sink/*.cpp
	sink/rtp/*.cpp
	sink/appsink/*.cpp
)

file(GLOB CONFIG_SOURCE_LIST 
	config/*.cpp
)

file(GLOB SESSION_SOURCE_LIST 
	session/*.cpp
)

file(GLOB PLATFORM_SOURCE_LIST 
	capture/*.cpp
	capture/windows/*.cpp
	capture/windows/gpu/*.cpp
	capture/windows/duplication/*.cpp
)


set(SCREENCODER_TARGET_FILES
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

  ${CMAKE_CURRENT_SOURCE_DIR}/encode
  ${CMAKE_CURRENT_SOURCE_DIR}/encode/encoder/d3d11
  ${CMAKE_CURRENT_SOURCE_DIR}/encode/encoder

  ${CMAKE_CURRENT_SOURCE_DIR}/input
  ${CMAKE_CURRENT_SOURCE_DIR}/sink
  ${CMAKE_CURRENT_SOURCE_DIR}/sink/rtp
  ${CMAKE_CURRENT_SOURCE_DIR}/sink/appsink
  ${CMAKE_CURRENT_SOURCE_DIR}/sink/appsink/go
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

  ${CMAKE_CURRENT_SOURCE_DIR}/capture
  ${CMAKE_CURRENT_SOURCE_DIR}/capture/windows
  ${CMAKE_CURRENT_SOURCE_DIR}/capture/windows/gpu
  ${CMAKE_CURRENT_SOURCE_DIR}/capture/windows/duplication

  ${FFMPEG_INCLUDE_DIRS}
  ${PLATFORM_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIR}
)
 



list(APPEND SCREENCODER_EXTERNAL_LIBRARIES
    ${CBS_EXTERNAL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${FFMPEG_LIBRARIES}
    ${PLATFORM_LIBRARIES}
    ${Boost_LIBRARIES}
)





add_library(screencoderlib STATIC
	${SCREENCODER_TARGET_FILES}
)


target_link_libraries(screencoderlib ${SCREENCODER_EXTERNAL_LIBRARIES})
target_compile_definitions(screencoderlib PUBLIC ${SCREENCODER_DEFINITIONS})
set_target_properties(screencoderlib PROPERTIES CXX_STANDARD 17
                               VERSION ${PROJECT_VERSION}
                               SOVERSION ${PROJECT_VERSION_MAJOR})

add_executable(screencoder 
	examples/rtp/main.cpp
)

target_link_libraries(screencoder 
	${CMAKE_CURRENT_BINARY_DIR}/libscreencoderlib.a
	${SCREENCODER_EXTERNAL_LIBRARIES}
)
target_compile_definitions(screencoder PUBLIC ${SCREENCODER_DEFINITIONS})
set_target_properties(screencoder PROPERTIES CXX_STANDARD 17
                               VERSION ${PROJECT_VERSION}
                               SOVERSION ${PROJECT_VERSION_MAJOR})