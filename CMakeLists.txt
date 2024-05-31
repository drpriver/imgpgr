cmake_minimum_required(VERSION 3.14) # This is a guess
project(imgpgr VERSION 1.0.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

if(WIN32)
add_compile_options("-D_CRT_NONSTDC_NO_DEPRECATE" "-D_CRT_SECURE_NO_WARNINGS")
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
add_compile_options("/wd4244" "/wd5105" "/wd4146" "/wd4267" "/wd4018" "/wd4141" "/wd4163")
endif()
elseif(APPLE)
elseif(UNIX)
set(LIBM_LIBRARIES m)
endif()

add_executable(imgpgr imgpgr.c)
target_link_libraries(imgpgr ${LIBM_LIBRARIES})

install(TARGETS imgpgr DESTINATION bin)

