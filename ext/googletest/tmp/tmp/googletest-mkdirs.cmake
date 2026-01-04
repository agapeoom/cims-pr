# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/nex/work/cims/ext/googletest/src"
  "/home/nex/work/cims/ext/googletest"
  "/home/nex/work/cims/ext/googletest"
  "/home/nex/work/cims/ext/googletest/tmp/tmp"
  "/home/nex/work/cims/ext/googletest/tmp/src/googletest-stamp"
  "/home/nex/work/cims/ext/googletest/tmp/src"
  "/home/nex/work/cims/ext/googletest/tmp/src/googletest-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/nex/work/cims/ext/googletest/tmp/src/googletest-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/nex/work/cims/ext/googletest/tmp/src/googletest-stamp${cfgdir}") # cfgdir has leading slash
endif()
