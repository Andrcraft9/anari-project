add_executable(demo)
target_compile_features(demo PUBLIC cxx_std_20)

find_package(anari CONFIG REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

target_include_directories(
  demo
  PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/src"
)
target_link_libraries(
  demo
  PUBLIC
    anari::anari
    anari::helium
    anari::anari_test_scenes
    anari::anari_library_sink
    ${CMAKE_THREAD_LIBS_INIT}
    Threads::Threads
)

add_subdirectory(src)
