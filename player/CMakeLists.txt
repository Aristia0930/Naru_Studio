find_package(SDL3 3.2 REQUIRED CONFIG COMPONENTS SDL3-shared)
find_package(SDL3_ttf 3.2 REQUIRED CONFIG)

add_executable(naru_player main.cpp)

set_target_properties(naru_player PROPERTIES
  OUTPUT_NAME NaruPlayer
  WIN32_EXECUTABLE ON
)
target_link_libraries(naru_player PRIVATE naru_engine SDL3::SDL3 SDL3_ttf::SDL3_ttf)

if(NARU_BUILD_PYTHON)
  target_link_libraries(naru_player PRIVATE naru_python)
  target_compile_definitions(naru_player PRIVATE NARU_PLAYER_HAS_PYTHON=1)
  naru_copy_python_runtime(naru_player)
else()
  target_compile_definitions(naru_player PRIVATE NARU_PLAYER_HAS_PYTHON=0)
endif()

# SDL3.dll, SDL3_ttf.dll을 실행 파일 옆에 둔다.
if(WIN32)
  add_custom_command(TARGET naru_player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:naru_player> $<TARGET_FILE_DIR:naru_player>
    COMMAND_EXPAND_LISTS
    VERBATIM)
endif()
