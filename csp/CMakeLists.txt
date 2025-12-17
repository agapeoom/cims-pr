cmake_minimum_required(VERSION 3.10)
project(csp)

set(CMAKE_CXX_STANDARD 11)

# Find common system prerequisites
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Add the dependency libraries from ext/psip
# We assume this file is in .../csp/ and psip is in .../ext/psip/
# Adjust path if necessary. Using relative path here.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ext/psip ${CMAKE_CURRENT_BINARY_DIR}/psip_build)

# Include current directory source files
file(GLOB SRCS "*.cpp")

# Define the executable
add_executable(csp ${SRCS})

# Include directories (csp own headers)
target_include_directories(csp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link against the psip libraries
# Note: The order here should match the dependency stack if not automatically handled,
# but using target links usually handles it.
target_link_libraries(csp PUBLIC
    sipuseragent
    sipstack
    sipparser
    sdpparser
    xmlparser
    serverplatform
    sipplatform
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    OpenSSL::Crypto
)

# --- Deployment / Install Configuration ---

# Set default install prefix to 'dist' folder in source root if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/dist" CACHE PATH "Default install prefix" FORCE)
endif()

# 1. Binaries (bin/)
install(TARGETS csp DESTINATION bin)
install(PROGRAMS csp.sh DESTINATION bin)

# 2. Configuration (config/)
install(FILES csp.xml DESTINATION config)
install(DIRECTORY SipServerXml UserXml cdr DESTINATION config)

# 3. Certificates (cert/)
# Install csp.pem from the build directory (where it was generated)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/csp.pem DESTINATION cert)

# 4. Logs (log/)
# Create empty log directory
install(DIRECTORY DESTINATION log)
