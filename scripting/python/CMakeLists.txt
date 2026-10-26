find_package(Python3 3.13 REQUIRED COMPONENTS Interpreter Development.Embed)

add_library(naru_python STATIC
  PythonRuntime.cpp
)

target_include_directories(naru_python PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(naru_python PUBLIC naru_engine PRIVATE Python3::Python)

# 개발 빌드에서는 설치된 Python의 표준 라이브러리를 사용한다. 실행 시 NARU_PYTHON_HOME 환경변수로 바꿀 수 있다.
# 배포 빌드(M5)에서는 player 옆에 런타임을 묶고 이 기본값을 쓰지 않는다.
cmake_path(GET Python3_EXECUTABLE PARENT_PATH NARU_PYTHON_DEFAULT_HOME)
target_compile_definitions(naru_python PRIVATE
  NARU_PYTHON_DEFAULT_HOME="${NARU_PYTHON_DEFAULT_HOME}"
)

# python3xx.dll을 실행 파일 폴더로 복사하는 데 사용할 경로.
set(NARU_PYTHON_RUNTIME_DLLS "" CACHE INTERNAL "")
if(WIN32)
  file(GLOB _naru_py_dlls "${NARU_PYTHON_DEFAULT_HOME}/python3*.dll")
  set(NARU_PYTHON_RUNTIME_DLLS "${_naru_py_dlls}" CACHE INTERNAL "")
endif()

function(naru_copy_python_runtime target)
  foreach(dll IN LISTS NARU_PYTHON_RUNTIME_DLLS)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:${target}>"
      VERBATIM)
  endforeach()
endfunction()
