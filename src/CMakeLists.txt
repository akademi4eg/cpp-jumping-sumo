cmake_minimum_required(VERSION 3.6)
project(sumo_runner)

set(SUMO_PATH /home/akademi4eg/Lab/sumo)

set(CMAKE_CXX_STANDARD 11)

message(STATUS "SDK location: ${SUMO_PATH}")

include_directories("${CMAKE_SOURCE_DIR}")
include_directories("${SUMO_PATH}/out/arsdk-native/staging/usr/include/")
link_directories("${SUMO_PATH}/out/arsdk-native/staging/usr/lib/")

set(SOURCE_FILES main.cpp CDeviceController.cpp)
add_executable(sumo_runner ${SOURCE_FILES})

target_link_libraries(sumo_runner -larsal -lardiscovery -larcontroller)