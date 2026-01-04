# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/nex/work/cims/ext/oneTBB_down/src/oneTBB"
  "/home/nex/work/cims/ext/oneTBB_down/src/oneTBB-build"
  "/home/nex/work/cims/ext/oneTBB_down"
  "/home/nex/work/cims/ext/oneTBB_down/tmp"
  "/home/nex/work/cims/ext/oneTBB_down/src/oneTBB-stamp"
  "/home/nex/work/cims/ext/oneTBB_down/src"
  "/home/nex/work/cims/ext/oneTBB_down/src/oneTBB-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/nex/work/cims/ext/oneTBB_down/src/oneTBB-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/nex/work/cims/ext/oneTBB_down/src/oneTBB-stamp${cfgdir}") # cfgdir has leading slash
endif()
