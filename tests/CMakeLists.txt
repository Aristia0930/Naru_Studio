add_executable(naru_engine_tests engine_tests.cpp)
target_link_libraries(naru_engine_tests PRIVATE naru_engine)
add_test(NAME engine COMMAND naru_engine_tests)

if(NARU_BUILD_PYTHON)
  add_executable(naru_python_tests python_tests.cpp)
  target_link_libraries(naru_python_tests PRIVATE naru_python)
  target_compile_definitions(naru_python_tests PRIVATE
    NARU_TEST_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/scripts"
  )
  naru_copy_python_runtime(naru_python_tests)
  add_test(NAME python COMMAND naru_python_tests)
endif()

if(NARU_BUILD_EDITOR)
  # 창을 띄운 뒤 일정 시간 후 스스로 닫히는지 확인하는 스모크 테스트.
  add_test(NAME editor_smoke COMMAND naru_editor --smoke-test 1500)
endif()
