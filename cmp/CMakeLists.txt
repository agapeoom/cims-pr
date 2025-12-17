#
#Project("pmproc library" C CXX)

include_directories(
   ./
   ${PMP_INC}
   ${PMPROC_INC}
)

#   ${FFM_LIB_PATH}
message(" 111 : ${FFM_LIB_PATH}")
message(" ###> ${PMPROC_BIN_PATH}")
message(" ***> ${PMPROC_LIB_PATH}")
#message("${FFM_LIB}")


add_executable(pmpagent pmpagent.cpp)
add_executable(prtpsim prtpsim.cpp)

set(TEST_SRCS
   pmpagent 
   prtpsim 
)


install(TARGETS ${TEST_SRCS}  
	    RUNTIME DESTINATION ${PMPROC_BIN_PATH}
		LIBRARY DESTINATION ${PMPROC_LIB_PATH}
		ARCHIVE DESTINATION ${PMPROC_LIB_PATH})


