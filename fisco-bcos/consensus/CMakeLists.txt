file(GLOB SRC_LIST "*.cpp")
file(GLOB HEADERS "*.h")

add_executable(mini-consensus ${SRC_LIST} ${HEADERS})

target_link_libraries(mini-consensus PUBLIC initializer)
