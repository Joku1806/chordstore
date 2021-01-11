cmake_minimum_required(VERSION 3.16)
project(Block4 C)
set(CMAKE_C_STANDARD 99)
add_compile_options(-O3)

add_executable(client client.c protocol.c VLA.c bytebuffer.c)
target_link_libraries(client m)
add_executable(peer peer.c protocol.c VLA.c bytebuffer.c datastore.c)
target_link_libraries(peer m)