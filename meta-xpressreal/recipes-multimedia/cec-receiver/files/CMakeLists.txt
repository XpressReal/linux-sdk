cmake_minimum_required(VERSION 3.10)
project(cec_receiver C)

add_executable(cec_receiver cec_receiver.c)

target_link_libraries(cec_receiver 
    pthread
)

target_compile_options(cec_receiver PRIVATE
	-g
    -Wall 
    -Wextra 
    -Werror
    -std=gnu99
)

install(TARGETS cec_receiver DESTINATION ${CMAKE_INSTALL_BINDIR})
