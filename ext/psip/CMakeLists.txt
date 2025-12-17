cmake_minimum_required(VERSION 3.10)
project(psip)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find dependencies (OpenSSL is commonly used in SIP stacks)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Helper macro to create static libraries from subdirectories
macro(create_psip_lib lib_name dir_name)
    file(GLOB_RECURSE ${lib_name}_SOURCES "${dir_name}/*.cpp")
    add_library(${lib_name} STATIC ${${lib_name}_SOURCES})
    target_include_directories(${lib_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${dir_name})
    # Most components need these common includes and libs
    target_link_libraries(${lib_name} PUBLIC Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
endmacro()

# Create libraries for each component
# Order doesn't strictly matter for definition, but logical dependencies usually form a stack
create_psip_lib(sipplatform    SipPlatform)
create_psip_lib(sipparser      SipParser)
create_psip_lib(sdpparser      SdpParser)
create_psip_lib(xmlparser      XmlParser)
create_psip_lib(sipstack       SipStack)
create_psip_lib(sipuseragent   SipUserAgent)
create_psip_lib(serverplatform ServerPlatform)

# Establish dependencies to ensure correct include path propagation
# Base platform lib used by almost everyone
target_link_libraries(sipparser      PUBLIC sipplatform)
target_link_libraries(sdpparser      PUBLIC sipplatform)
target_link_libraries(xmlparser      PUBLIC sipplatform)
target_link_libraries(serverplatform PUBLIC sipplatform)

# SipStack depends on parsers and platform
target_link_libraries(sipstack PUBLIC sipplatform sipparser sdpparser xmlparser)

# SipUserAgent depends on SipStack and ServerPlatform
target_link_libraries(sipuseragent PUBLIC sipstack serverplatform)

# Expose all libraries as a convenient target if needed, slightly redundant but harmless
add_custom_target(psip COMMENTS "Building all psip libraries" DEPENDS 
    sipplatform sipparser sdpparser xmlparser sipstack sipuseragent serverplatform
)
