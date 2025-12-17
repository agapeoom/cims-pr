
add_library(${PMPROC_LIB} STATIC
   PAVFilter.cpp
   PAVBuffer.cpp
   PAVSession.cpp
   PAVRtpSession.cpp
   PAVRtpSimSession.cpp
   PMPManager.cpp
   havrtputil.cpp
)

#install(TARGETS ${PMPROC_LIB}
#        RUNTIME DESTINATION ${PMPROC_BIN_PATH}
#        LIBRARY DESTINATION ${PMPROC_LIB_PATH}
#        ARCHIVE DESTINATION ${PMPROC_LIB_PATH})

