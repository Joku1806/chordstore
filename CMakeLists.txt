project(Block3 C)
set(CMAKE_C_STANDARD 99)
set (CMAKE_C_FLAGS "-g -O2")

add_executable(client client.c protocol.c bytebuffer.c VLA.c)
add_executable(server server.c protocol.c bytebuffer.c datastore.c)
