# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/nex/work/cims/pkg/l_ffmpeg/src"
  "/home/nex/work/cims/pkg/l_ffmpeg"
  "/home/nex/work/cims/pkg/l_ffmpeg"
  "/home/nex/work/cims/ffmpeg-prefix/tmp"
  "/home/nex/work/cims/ffmpeg-prefix/src/ffmpeg-stamp"
  "/home/nex/work/cims/pkg/l_ffmpeg"
  "/home/nex/work/cims/ffmpeg-prefix/src/ffmpeg-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/nex/work/cims/ffmpeg-prefix/src/ffmpeg-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/nex/work/cims/ffmpeg-prefix/src/ffmpeg-stamp${cfgdir}") # cfgdir has leading slash
endif()
