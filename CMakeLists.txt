cmake_minimum_required(VERSION 3.25)

project(NaruStudio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 모든 실행 파일과 DLL을 한 폴더에 모아 개발 중 바로 실행할 수 있게 한다.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(NARU_BUILD_PYTHON "Python 게임 스크립트 연결을 빌드한다" ON)
option(NARU_BUILD_EDITOR "Qt 제작툴을 빌드한다" ON)
option(NARU_BUILD_PLAYER "SDL3 게임 실행 화면(NaruPlayer)을 빌드한다" ON)
option(NARU_BUILD_TESTS "테스트를 빌드한다" ON)

if(MSVC)
  add_compile_options(/utf-8 /W4 /permissive- /Zc:__cplusplus)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(engine)

if(NARU_BUILD_PYTHON)
  add_subdirectory(scripting/python)
endif()

if(NARU_BUILD_PLAYER)
  add_subdirectory(player)
endif()

if(NARU_BUILD_EDITOR)
  add_subdirectory(editor)
endif()

if(NARU_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
