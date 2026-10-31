add_executable(naru_engine_tests engine_tests.cpp)
target_link_libraries(naru_engine_tests PRIVATE naru_engine)
add_test(NAME engine COMMAND naru_engine_tests)

add_executable(naru_event_tests event_tests.cpp)
target_link_libraries(naru_event_tests PRIVATE naru_engine)
add_test(NAME events COMMAND naru_event_tests)

add_executable(naru_database_tests database_tests.cpp)
target_link_libraries(naru_database_tests PRIVATE naru_engine)
add_test(NAME database COMMAND naru_database_tests)

add_executable(naru_game_tests game_tests.cpp)
target_link_libraries(naru_game_tests PRIVATE naru_engine)
add_test(NAME game COMMAND naru_game_tests)

add_executable(naru_project_tests project_tests.cpp)
target_link_libraries(naru_project_tests PRIVATE naru_engine)
add_test(NAME project COMMAND naru_project_tests)

if(NARU_BUILD_PYTHON)
  add_executable(naru_python_tests python_tests.cpp)
  target_link_libraries(naru_python_tests PRIVATE naru_python)
  target_compile_definitions(naru_python_tests PRIVATE
    NARU_TEST_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/scripts"
    NARU_SCRIPT_API_DOC="${PROJECT_SOURCE_DIR}/docs/SCRIPT_API.md"
    NARU_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/docs/examples"
  )
  naru_copy_python_runtime(naru_python_tests)
  add_test(NAME python COMMAND naru_python_tests)
endif()

if(NARU_BUILD_EDITOR)
  # 매번 새 프로젝트를 만들어 연다.
  set(_naru_smoke_project ${CMAKE_BINARY_DIR}/test_projects/smoke)
  add_test(NAME editor_smoke_prepare COMMAND ${CMAKE_COMMAND} -E rm -rf ${_naru_smoke_project})
  # 창을 띄운 뒤 일정 시간 후 스스로 닫히는지 확인한다.
  add_test(NAME editor_smoke COMMAND naru_editor --new-project ${_naru_smoke_project} --smoke-test 1500)
  # 객체 추가·이동·Undo·저장·재로드·스크립트 실행을 실제 창에서 검사한다.
  add_test(NAME editor_selftest COMMAND naru_editor --new-project ${_naru_smoke_project} --self-test)
  set_tests_properties(editor_smoke PROPERTIES DEPENDS editor_smoke_prepare)
  set_tests_properties(editor_selftest PROPERTIES DEPENDS editor_smoke)
endif()

if(NARU_BUILD_PLAYER AND NARU_BUILD_PYTHON)
  find_package(Python3 3.13 REQUIRED COMPONENTS Interpreter)
  add_test(NAME player_choices COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/player_choices.py $<TARGET_FILE:naru_player>
    ${PROJECT_SOURCE_DIR}/docs/examples/choices)
  set_tests_properties(player_choices PROPERTIES TIMEOUT 45)
endif()
