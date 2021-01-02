project(Block3 C)
set(CMAKE_C_STANDARD 99)
set (CMAKE_C_FLAGS "-g -O2")

add_executable(client client.c protocol.c VLA.c bytebuffer.c)
add_executable(peer peer.c protocol.c VLA.c bytebuffer.c datastore.c)
