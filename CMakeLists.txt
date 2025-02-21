cmake_minimum_required(VERSION 3.19)

set(
  ANARI_PROJECT_VERSION
  0.0.1
  CACHE INTERNAL
  "Version number of the ANARI Project")

project(
  ANARIProject
  DESCRIPTION "ANARI Project"
  VERSION ${ANARI_PROJECT_VERSION}
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(examples)
