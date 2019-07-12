file(GLOB SRC_LIST "*.cpp")
file(GLOB HEADERS "*.h")

add_executable(mini-evm ${SRC_LIST} ${HEADERS})

target_link_libraries(mini-evm PUBLIC initializer)
