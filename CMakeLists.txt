cmake_minimum_required(VERSION 2.8)
project(arucoTest)
SET(CMAKE_MODULE_PATH ${CMAKE_INSTALL_PREFIX}/lib/cmake/ )
set(HEADERS
   server.h
)
set(SOURCE 
   arucoTest.cpp
)
find_package(aruco REQUIRED )
FIND_PACKAGE( OpenCV REQUIRED )

find_path(${LIMBS}/include/pthread pthread.h)
FIND_LIBRARY(${LIMBS}/lib pthreadVCE2)

set(LIMBS_ALL ${LIMBS_ALL} ${aruco_LIBS} pthreadVCE2)

add_executable(arucoTest ${SOURCE} ${HEADERS})
target_link_libraries(arucoTest ${LIMBS_ALL})