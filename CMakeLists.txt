cmake_minimum_required(VERSION 2.8)
project(arucoTest)
SET(CMAKE_MODULE_PATH ${CMAKE_INSTALL_PREFIX}/lib/cmake/ )

if(WIN32)
    # Postfix of DLLs:
	find_path(${LIMBS}/include/pthread pthread.h)
	FIND_LIBRARY(${LIMBS}/lib pthreadVCE2)
	set(LIMBS_ALL ${LIMBS_ALL} ${aruco_LIBS} pthreadVCE2)
else()
    # Postfix of so's:
    set(LIMBS_ALL ${LIMBS_ALL} ${aruco_LIBS})
endif()

set(HEADERS
   Server.h
)
set(SOURCES 
   arucoTest.cpp
   Server.cpp
)
find_package(aruco REQUIRED )
FIND_PACKAGE( OpenCV REQUIRED )

add_executable(arucoTest ${SOURCES} ${HEADERS})
target_link_libraries(arucoTest ${LIMBS_ALL})
