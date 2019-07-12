file(GLOB SRC_LIST "*.cpp")
file(GLOB HEADERS "*.h")
add_executable(mini-verifier ${SRC_LIST} ${HEADERS})
target_link_libraries(mini-verifier PUBLIC initializer)
