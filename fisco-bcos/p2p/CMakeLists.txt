file(GLOB SRC_LIST "*.cpp")
file(GLOB HEADERS "*.h")

add_executable(mini-p2p ${SRC_LIST} ${HEADERS})

target_link_libraries(mini-p2p PUBLIC initializer)
