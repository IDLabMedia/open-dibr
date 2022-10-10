cmake_minimum_required(VERSION 3.7)

project(RealtimeDIBR)

# Set C++11 for all projects and disable non-standard extensions
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INSTALL_PREFIX .)


set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(REALTIME_DIBR_INSTALL_DIR ${CMAKE_BINARY_DIR})

# OpenVR headers
set(OPENVR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/OpenVR)
set(OPENVR_INCLUDE_DIRS
 ${OPENVR_DIR}/headers
 ${OPENVR_DIR}/samples/thirdparty/glew/glew-1.11.0/include
 ${OPENVR_DIR}/samples/thirdparty/sdl2-2.0.3/include
 ${OPENVR_DIR}/samples/thirdparty/vulkan-1.0.49.0/include
)

# GLM headers
set(GLM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/glm/glm)

# NVIDIA video codec libs
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        Set(CUVID_LIB ${LIB_DIR}/x64/nvcuvid.lib) 
        set(NVENCODEAPI_LIB ${LIB_DIR}/x64/nvencodeapi.lib)  
    else()
        Set(CUVID_LIB ${LIB_DIR}/Win32/nvcuvid.lib) 
        set(NVENCODEAPI_LIB ${LIB_DIR}/Win32/nvencodeapi.lib) 
    endif()
else ()
    find_library(CUVID_LIB nvcuvid)
    find_library(NVENCODEAPI_LIB nvidia-encode)
endif()

# FFMPEG LibAV libs
if(WIN32)
    set(NV_FFMPEG_HDRS ${INCLUDE_DIR}/FFmpeg/include)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        Set(AVCODEC_LIB ${INCLUDE_DIR}/FFmpeg/lib/x64/avcodec.lib)
        Set(AVFORMAT_LIB ${INCLUDE_DIR}/FFmpeg/lib/x64/avformat.lib)
        Set(AVUTIL_LIB ${INCLUDE_DIR}/FFmpeg/lib/x64/avutil.lib)
        Set(SWRESAMPLE_LIB ${INCLUDE_DIR}/FFmpeg/lib/x64/swresample.lib)
        Set(FFMPEG_DLL_DIR ${INCLUDE_DIR}/FFmpeg/lib/x64/)
    else()
        Set(AVCODEC_LIB ${INCLUDE_DIR}/FFmpeg/lib/Win32/avcodec.lib)
        Set(AVFORMAT_LIB ${INCLUDE_DIR}/FFmpeg/lib/Win32/avformat.lib)
        Set(AVUTIL_LIB ${INCLUDE_DIR}/FFmpeg/lib/Win32/avutil.lib)
        Set(SWRESAMPLE_LIB ${INCLUDE_DIR}/FFmpeg/lib/Win32/swresample.lib)
        Set(FFMPEG_DLL_DIR ${INCLUDE_DIR}/FFmpeg/lib/Win32/)
    endif()
    #copy dll's
    install(DIRECTORY ${FFMPEG_DLL_DIR} DESTINATION ${REALTIME_DIBR_INSTALL_DIR} FILES_MATCHING PATTERN "*.dll")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PC_AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(PC_AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(PC_AVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(PC_SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

    set(NV_FFMPEG_HDRS ${PC_AVCODEC_INCLUDE_DIRS})
    find_library(AVCODEC_LIBRARY NAMES avcodec
            HINTS
            ${PC_AVCODEC_LIBDIR}
            ${PC_AVCODEC_LIBRARY_DIRS}
            )
    find_library(AVFORMAT_LIBRARY NAMES avformat
            HINTS
            ${PC_AVFORMAT_LIBDIR}
            ${PC_AVFORMAT_LIBRARY_DIRS}
            )
    find_library(AVUTIL_LIBRARY NAMES avutil
            HINTS
            ${PC_AVUTIL_LIBDIR}
            ${PC_AVUTIL_LIBRARY_DIRS}
            )
    find_library(SWRESAMPLE_LIBRARY NAMES swresample
            HINTS
            ${PC_SWRESAMPLE_LIBDIR}
            ${PC_SWRESAMPLE_LIBRARY_DIRS}
            )
    set(AVCODEC_LIB ${AVCODEC_LIBRARY})
    set(AVFORMAT_LIB ${AVFORMAT_LIBRARY})
    set(AVUTIL_LIB ${AVUTIL_LIBRARY})
    set(SWRESAMPLE_LIB ${SWRESAMPLE_LIBRARY})
endif()

# SDL2, OpenGL, GLEW
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		Set(OPENVR_LIB_DIRS
		 ${OPENVR_DIR}/samples/thirdparty/glew/glew-1.11.0/lib/Release/x64/glew32.lib
		 ${OPENVR_DIR}/samples/thirdparty/sdl2-2.0.3/bin/win64/SDL2.lib
		 ${OPENVR_DIR}/samples/thirdparty/sdl2-2.0.3/bin/win64/SDL2main.lib
		 ${OPENVR_DIR}/lib/win64/openvr_api.lib
		 opengl32.lib
		 glu32.lib
		)
		Set(OPENVR_DLL_DIR ${LIB_DIR}/x64/)
    else()
		Set(OPENVR_LIB_DIRS
		 ${OPENVR_DIR}/samples/thirdparty/glew/glew-1.11.0/lib/Release/Win32/glew32.lib
		 ${OPENVR_DIR}/samples/thirdparty/sdl2-2.0.3/bin/win32/SDL2.lib
		 ${OPENVR_DIR}/samples/thirdparty/sdl2-2.0.3/bin/win32/SDL2main.lib
		 ${OPENVR_DIR}/lib/win32/openvr_api.lib
		 opengl32.lib
		 glu32.lib
		)
		Set(OPENVR_DLL_DIR ${LIB_DIR}/Win32/)
    endif()
	#copy dll's
    install(DIRECTORY ${OPENVR_DLL_DIR} DESTINATION ${REALTIME_DIBR_INSTALL_DIR} FILES_MATCHING PATTERN "*.dll")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(OpenGL REQUIRED)
    find_package(SDL2 REQUIRED)
    find_package(GLEW REQUIRED)

    Set(OPENVR_LIB_DIRS
     ${OPENVR_DIR}/lib/linux64/libopenvr_api.so
    )
endif()


set(APP_SOURCES
 ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
 ${CMAKE_CURRENT_SOURCE_DIR}/src/NvDecoder.cpp
)

set(APP_HEADERS
 ${CMAKE_CURRENT_SOURCE_DIR}/src/Application.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/PCApplication.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/VRApplication.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/glHelper.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/CameraVisibilityHelper.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/ioHelper.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/Pool.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/shader.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/AppDecUtils.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/FFmpegDemuxer.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/NvDecoder.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/NvCodecUtils.h
 ${CMAKE_CURRENT_SOURCE_DIR}/src/MeasureFPS.h
)

set(APP_RESOURCES
 ${CMAKE_CURRENT_SOURCE_DIR}/src/vertex.fs
 ${CMAKE_CURRENT_SOURCE_DIR}/src/geometry.fs
 ${CMAKE_CURRENT_SOURCE_DIR}/src/fragment.fs
 ${CMAKE_CURRENT_SOURCE_DIR}/src/cameras_vertex.fs
 ${CMAKE_CURRENT_SOURCE_DIR}/src/cameras_frag.fs
 ${CMAKE_CURRENT_SOURCE_DIR}/src/copy_fragment.fs
 ${CMAKE_CURRENT_SOURCE_DIR}/src/copy_vertex.fs
 ${CMAKE_CURRENT_SOURCE_DIR}/src/copy_fragment_1output.fs
)

set(NV_DEC_HDRS
 ${INCLUDE_DIR}/nvdec/cuviddec.h
 ${INCLUDE_DIR}/nvdec/nvcuvid.h
)

source_group( "headers" FILES ${APP_HEADERS} ${NV_DEC_HDRS} )
source_group( "sources" FILES ${APP_SOURCES} )
source_group( "resources" FILES ${APP_RESOURCES} )

find_package(CUDA)

set(CUDA_HOST_COMPILER ${CMAKE_CXX_COMPILER})

set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};-gencode arch=compute_50,code=\"sm_50,compute_50\")
if ( CMAKE_COMPILER_IS_GNUCC )
    if(NOT "${CUDA_NVCC_FLAGS}" MATCHES "-std=c\\+\\+11" )
        list(APPEND CUDA_NVCC_FLAGS -std=c++11)
    endif()
endif()

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()


cuda_add_executable(${PROJECT_NAME}  ${APP_SOURCES} ${APP_HEADERS} ${NV_DEC_HDRS} ${APP_RESOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES CUDA_SEPARABLE_COMPILATION ON)

target_include_directories(${PROJECT_NAME} PUBLIC ${CUDA_INCLUDE_DIRS}
 ${INCLUDE_DIR}/nvdec
 ${NV_FFMPEG_HDRS}
 ${INCLUDE_DIR}/stb_image
 ${INCLUDE_DIR}/cxxopts
 ${INCLUDE_DIR}/nlohmann
 ${OPENVR_INCLUDE_DIRS}
 ${GLM_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} ${CUDA_CUDA_LIBRARY} ${CMAKE_DL_LIBS} ${NVENCODEAPI_LIB} ${CUVID_LIB} ${AVCODEC_LIB}
 ${AVFORMAT_LIB} ${AVUTIL_LIB} ${SWRESAMPLE_LIB} ${OPENVR_LIB_DIRS}
 ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES} ${GLEW_LIBRARIES})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${REALTIME_DIBR_INSTALL_DIR})
if (MSVC)
	# copy .libs and .dlls to the CONFIG directory after building
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_target_properties( ${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${REALTIME_DIBR_INSTALL_DIR}/$<CONFIG>/ )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD  COMMAND ${CMAKE_COMMAND} -E copy_directory ${FFMPEG_DLL_DIR} ${REALTIME_DIBR_INSTALL_DIR}/$<CONFIG>/)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD  COMMAND ${CMAKE_COMMAND} -E copy_directory ${OPENVR_DLL_DIR} ${REALTIME_DIBR_INSTALL_DIR}/$<CONFIG>/)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC CMAKELISTS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
