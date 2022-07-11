cmake_minimum_required(VERSION 3.0)

project(SunshineSDK VERSION 0.1.0 DESCRIPTION "Sunshine is a Gamestream host for Moonlight.")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(SUNSHINE_SOURCE_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")


# Ugly hack to compile with #include <qos2.h>
add_compile_definitions(
	QOS_FLOWID=UINT32
	PQOS_FLOWID=UINT32*
	QOS_NON_ADAPTIVE_FLOW=2)

set(UPNPC_BUILD_SHARED OFF CACHE BOOL "no shared libraries")
set(UPNPC_BUILD_TESTS OFF CACHE BOOL "Don't build tests for miniupnpc")
set(UPNPC_BUILD_SAMPLE OFF CACHE BOOL "Don't build samples for miniupnpc")
set(UPNPC_NO_INSTALL ON CACHE BOOL "Don't install any libraries build for miniupnpc")

# uncomment for new build
# add_subdirectory(third-party/miniupnp/miniupnpc)
# add_subdirectory(third-party/moonlight-common-c/enet)
# add_subdirectory(third-party/Simple-Web-Server)

include_directories(third-party/miniupnp)


find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

list(APPEND SUNSHINE_COMPILE_OPTIONS -Wall -Wno-missing-braces -Wno-maybe-uninitialized -Wno-sign-compare)




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

add_compile_definitions(SUNSHINE_PLATFORM="windows")

# TODO
# add_subdirectory(tools) #This is temporary, only tools for Windows are needed, for now



set(PLATFORM_INCLUDE_DIR
	${SUNSHINE_PREPARED_BINARIES}/platform
	${SUNSHINE_PREPARED_BINARIES}/platform/windows
    )

set(FFMPEG_INCLUDE_DIRS
	${SUNSHINE_PREPARED_BINARIES}/include)

set(FFMPEG_LIBRARIES
	${SUNSHINE_PREPARED_BINARIES}/lib/libavcodec.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libavdevice.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libavfilter.a
	${SUNSHINE_PREPARED_BINARIES}/lib/libavformat.a
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
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB UTIL_SOURCE_LIST 
	util/*.cpp
	util/queue/*.cpp
	util/object/*.cpp
	util/macro/*.cpp
	util/log/*.cpp
	util/event/*.cpp
	util/error/*.cpp
	util/datatype/*.cpp
	util/avcodec/*.cpp
	util/array/*.cpp
)

file(GLOB ENCODER_SOURCE_LIST 
	encoder/*.cpp
	d3d11/*.cpp
	bitstream/*.cpp
)

file(GLOB RTP_SOURCE_LIST 
	rtp/*.cpp
)

file(GLOB CONFIG_SOURCE_LIST 
	config/*.cpp
)
file(GLOB PLATFORM_SOURCE_LIST 
	platform/*.cpp
	platform/windows/*.cpp
)

file(GLOB THIRDPARTY_SOURCE_LIST 
	third-party/moonlight-common-c/reedsolomon/rs.c
	third-party/moonlight-common-c/src/RtspParser.c
)

set(SUNSHINE_TARGET_FILES
	${UTIL_SOURCE_LIST}
	${ENCODER_SOURCE_LIST}
	${RTP_SOURCE_LIST}
	${CONFIG_SOURCE_LIST}
	${THIRDPARTY_SOURCE_LIST}
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

  ${CMAKE_CURRENT_SOURCE_DIR}/util
  ${CMAKE_CURRENT_SOURCE_DIR}/util/queue
  ${CMAKE_CURRENT_SOURCE_DIR}/util/object
  ${CMAKE_CURRENT_SOURCE_DIR}/util/macro
  ${CMAKE_CURRENT_SOURCE_DIR}/util/log
  ${CMAKE_CURRENT_SOURCE_DIR}/util/event
  ${CMAKE_CURRENT_SOURCE_DIR}/util/error
  ${CMAKE_CURRENT_SOURCE_DIR}/util/datatype
  ${CMAKE_CURRENT_SOURCE_DIR}/util/avcodec
  ${CMAKE_CURRENT_SOURCE_DIR}/util/array

  ${CMAKE_CURRENT_SOURCE_DIR}/platform
  ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows

  ${CMAKE_CURRENT_SOURCE_DIR}/third-party
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/cbs/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/moonlight-common-c/enet/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/moonlight-common-c/reedsolomon
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/moonlight-common-c/src
  
  ${FFMPEG_INCLUDE_DIRS}
  ${PLATFORM_INCLUDE_DIRS}
)


string(TOUPPER "x${CMAKE_BUILD_TYPE}" BUILD_TYPE)
if("${BUILD_TYPE}" STREQUAL "XDEBUG")
	list(APPEND SUNSHINE_COMPILE_OPTIONS -O0 -ggdb3)
	if(WIN32)
		set_source_files_properties(sunshine/src/http/nvhttp.cpp 
			PROPERTIES COMPILE_FLAGS -O2)
	endif()
else()
	add_definitions(-DNDEBUG)
	list(APPEND SUNSHINE_COMPILE_OPTIONS -O3)
endif()


list(APPEND CBS_EXTERNAL_LIBRARIES
	cbs)

list(APPEND SUNSHINE_EXTERNAL_LIBRARIES
    ${CBS_EXTERNAL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    enet
    opus
    ${FFMPEG_LIBRARIES}
    ${PLATFORM_LIBRARIES}
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

if(NOT DEFINED CMAKE_CUDA_STANDARD)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
endif()

foreach(flag IN LISTS SUNSHINE_COMPILE_OPTIONS)
	list(APPEND SUNSHINE_COMPILE_OPTIONS_CUDA "$<$<COMPILE_LANGUAGE:CUDA>:--compiler-options=${flag}>")
endforeach()

target_compile_options(sunshine PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${SUNSHINE_COMPILE_OPTIONS}>;$<$<COMPILE_LANGUAGE:CUDA>:${SUNSHINE_COMPILE_OPTIONS_CUDA};-std=c++17>)

