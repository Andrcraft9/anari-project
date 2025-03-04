add_executable(demo)
target_compile_features(demo PUBLIC cxx_std_20)

find_package(anari CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Stb REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

target_include_directories(
  demo
  PRIVATE
    ${Stb_INCLUDE_DIR}
)

target_link_libraries(
  demo
  PRIVATE
    anari::anari
    anari::helium
    anari::anari_test_scenes
    anari::anari_library_sink
    glfw
    GL
    ${CMAKE_THREAD_LIBS_INIT}
    Threads::Threads
)

add_subdirectory(src)
