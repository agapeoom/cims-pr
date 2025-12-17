#
#Project("pmproc library" C CXX)

include_directories(
   ./
   ${PMP_INC}
   ${PMPROC_INC}
#   ${GTEST_INC}
#   ${JT_INC}
#   ${TBB_INC}
#   ${FFM_INC}
#   ${CMAKE_CURRENT_SOURCE_DIR}/src
)

#link_directories(
#   ${GTEST_LIB_PATH}
#   ${JT_LIB_PATH}
#   ${TBB_LIB_PATH}
#   ${CMAKE_CURRENT_SOURCE_DIR}
#)

#   ${FFM_LIB_PATH}
message(" 111 : ${FFM_LIB_PATH}")
message(" ###> ${PMPROC_BIN_PATH}")
message(" ***> ${PMPROC_LIB_PATH}")
#message("${FFM_LIB}")


add_subdirectory(src)

set(TEST_SRCS
   pavtest 
   pavconv 
   pavagent 
   pmpagent
   prtpsim
)

set(FFM_OBJS
   /home/lmcuser/MPS/pmp/ext/ffmpeg-4.2.2/libavformat/utils.o
)

foreach(testsourcefile ${TEST_SRCS})
   message (add_executable( ${testsourcefile} ${testsourcefile}.cpp ) )
   add_executable( ${testsourcefile} test/${testsourcefile}.cpp )
   target_link_libraries( ${testsourcefile} ${FFM_LIB} ${TBB_LIB} ${COM_LIB} ${GTEST_LIB} )
   message("target_link_libraries( ${testsourcefile} ${FFM_LIB} ${TBB_LIB} ${COM_LIB} ${GTEST_LIB}" )
   #target_link_libraries( ${testsourcefile} )
endforeach(testsourcefile ${TEST_SRCS})

install(TARGETS ${TEST_SRCS}  
	    RUNTIME DESTINATION ${PMPROC_BIN_PATH}
		LIBRARY DESTINATION ${PMPROC_LIB_PATH}
		ARCHIVE DESTINATION ${PMPROC_LIB_PATH})

#foreach(testsourcefile ${TEST_SRCS})
#   message ("add_executable( ${testsourcefile} ${testsourcefile}.cpp )")
#   add_executable( ${testsourcefile} test/${testsourcefile}.cpp)
#   target_link_libraries(${testsourcefile} ${TBB_LIB} ${COM_LIB} ${GTEST_LIB} )
#endforeach(testsourcefile ${TEST_SRCS})



#install(TARGETS ${TEST_SRCS}
#   RUNTIME DESTINATION ${PMPROC_BIN_PATH}
#   LIBRARY DESTINATION ${PMPROC_LIB_PATH}
#   ARCHIVE DESTINATION ${PMPROC_LIB_PATH})



