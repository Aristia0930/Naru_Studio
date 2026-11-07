add_executable(naru_engine_tests engine_tests.cpp)
target_link_libraries(naru_engine_tests PRIVATE naru_engine)
add_test(NAME engine COMMAND naru_engine_tests)
add_executable(naru_input_tests input_tests.cpp)
target_link_libraries(naru_input_tests PRIVATE naru_engine)
add_test(NAME input COMMAND naru_input_tests)

add_executable(naru_event_tests event_tests.cpp)
target_link_libraries(naru_event_tests PRIVATE naru_engine)
target_compile_definitions(naru_event_tests PRIVATE NARU_EVENT_GUIDE_DOC="${PROJECT_SOURCE_DIR}/docs/guide/EVENTS.md")
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
  add_test(NAME python_guide_docs COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/python_guide_docs.py ${PROJECT_SOURCE_DIR})
  add_test(NAME player_images COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/player_images.py $<TARGET_FILE:naru_player>)
  set_tests_properties(player_images PROPERTIES TIMEOUT 60)
  add_test(NAME player_choices COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/player_choices.py $<TARGET_FILE:naru_player>
    ${PROJECT_SOURCE_DIR}/docs/examples/choices)
  set_tests_properties(player_choices PROPERTIES TIMEOUT 45)
  add_test(NAME player_common_events COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/player_common_events.py $<TARGET_FILE:naru_player>
    ${PROJECT_SOURCE_DIR}/docs/examples/common-events)
  set_tests_properties(player_common_events PROPERTIES TIMEOUT 45)
  add_test(NAME player_input_settings COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/player_input_settings.py $<TARGET_FILE:naru_player>)
  set_tests_properties(player_input_settings PROPERTIES TIMEOUT 45)
  add_test(NAME player_debug COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/player_debug.py $<TARGET_FILE:naru_player>
    ${PROJECT_SOURCE_DIR}/docs/examples/common-events)
  set_tests_properties(player_debug PROPERTIES TIMEOUT 45)
endif()

if(NARU_BUILD_EDITOR)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
  find_package(SDL3 3.2 REQUIRED CONFIG COMPONENTS SDL3-shared)
  qt_add_executable(naru_asset_editor_tests
    asset_editor_tests.cpp
    image_editor_tests.cpp
    ../editor/ImageSettingsDialog.cpp ../editor/ImageSettingsDialog.h
    ../editor/Inspector.cpp ../editor/Inspector.h
    ../editor/SceneView.cpp ../editor/SceneView.h
    ../editor/UndoCommands.cpp ../editor/UndoCommands.h
    ../editor/AssetBrowser.cpp ../editor/AssetBrowser.h
    ../editor/ProjectDocument.cpp ../editor/ProjectDocument.h)
  target_include_directories(naru_asset_editor_tests PRIVATE ../editor)
  set_target_properties(naru_asset_editor_tests PROPERTIES AUTOMOC ON)
  target_link_libraries(naru_asset_editor_tests PRIVATE naru_engine Qt6::Widgets SDL3::SDL3)
  if(NARU_BUILD_PLAYER)
    target_compile_definitions(naru_asset_editor_tests PRIVATE NARU_TEST_PLAYER_EXE="$<TARGET_FILE:naru_player>")
    add_dependencies(naru_asset_editor_tests naru_player)
  endif()
  add_dependencies(naru_asset_editor_tests naru_editor)
  add_test(NAME asset_editor COMMAND naru_asset_editor_tests)
endif()
