
CMAKE_MINIMUM_REQUIRED(VERSION 3.12.1)
PROJECT(openthread)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    link_libraries(ws2_32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
else()  
    link_libraries(pthread)
endif()

include_directories(src)

add_executable(test 
	src/openthread.h 
	src/openthread.cpp 
	test/main.cpp
)
