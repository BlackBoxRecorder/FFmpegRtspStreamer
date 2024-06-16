cmake_minimum_required (VERSION 3.8)

project ("FFmpegRtspStreamer")

set(CMAKE_CXX_STANDARD 17)

set(FFMPEG_LIBS_DIR "C:/yin-data/bin/ffmpeg-dev/lib")
set(FFMPEG_HEADERS_DIR "C:/yin-data/bin/ffmpeg-dev/include")

set(OPENCV_LIBS_DIR "C:/yin-data/bin/opencv-4.8.0-build/install/x64/vc17/lib")
set(OPENCV_HEADERS_DIR "C:/yin-data/bin/opencv-4.8.0-build/install/include")

link_directories(${FFMPEG_LIBS_DIR})
include_directories( ${FFMPEG_HEADERS_DIR} )

link_directories(${OPENCV_LIBS_DIR})
include_directories( ${OPENCV_HEADERS_DIR} )

add_executable (FFmpegRtspStreamer "FFmpegRtspStreamer.cpp")

target_link_libraries(FFmpegRtspStreamer
        opencv_world480d
        avformat
        avcodec
        avdevice
        avfilter
        avutil
        postproc
        swresample
        swscale
       
        )
