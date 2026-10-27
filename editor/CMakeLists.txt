if(NOT NARU_BUILD_PYTHON)
  message(FATAL_ERROR "Qt 제작툴은 Python 스크립트 연결이 필요합니다. NARU_BUILD_PYTHON=ON으로 설정하세요.")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(naru_editor
  main.cpp
  CommandDialog.cpp
  CommandDialog.h
  DatabaseEditor.cpp
  DatabaseEditor.h
  FieldDialog.cpp
  FieldDialog.h
  GlobalScriptsDialog.cpp
  GlobalScriptsDialog.h
  ValueEditor.cpp
  ValueEditor.h
  EventEditor.cpp
  EventEditor.h
  Inspector.cpp
  Inspector.h
  MainWindow.cpp
  MainWindow.h
  ProjectDocument.cpp
  ProjectDocument.h
  SceneView.cpp
  SceneView.h
  ScriptEditor.cpp
  ScriptEditor.h
  StartDialog.cpp
  StartDialog.h
  UndoCommands.cpp
  UndoCommands.h
)

set_target_properties(naru_editor PROPERTIES
  OUTPUT_NAME NaruStudio
  WIN32_EXECUTABLE ON
)
target_link_libraries(naru_editor PRIVATE naru_engine naru_python Qt6::Widgets)
target_compile_definitions(naru_editor PRIVATE NARU_STUDIO_VERSION="${PROJECT_VERSION}")

naru_copy_python_runtime(naru_editor)

# "게임 실행"은 같은 폴더의 NaruPlayer.exe를 띄우므로 함께 빌드한다.
if(TARGET naru_player)
  add_dependencies(naru_editor naru_player)
endif()

# 빌드한 뒤 바로 실행: cmake --build --preset windows-msvc --target run
# (마지막으로 연 프로젝트를 연다. IDE의 CMake 타깃 목록에서도 고를 수 있다)
add_custom_target(run
  COMMAND $<TARGET_FILE:naru_editor> --last
  DEPENDS naru_editor
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  USES_TERMINAL
  COMMENT "Naru Studio 실행")

# 개발 중 바로 실행할 수 있도록 Qt DLL·플러그인을 실행 파일 옆에 배치한다.
if(WIN32)
  get_target_property(_naru_qmake Qt6::qmake IMPORTED_LOCATION)
  cmake_path(GET _naru_qmake PARENT_PATH _naru_qt_bin)
  find_program(NARU_WINDEPLOYQT windeployqt HINTS "${_naru_qt_bin}" NO_DEFAULT_PATH)
  if(NARU_WINDEPLOYQT)
    add_custom_command(TARGET naru_editor POST_BUILD
      COMMAND "${NARU_WINDEPLOYQT}" --no-translations --no-compiler-runtime
              --no-system-d3d-compiler --no-opengl-sw --verbose 0
              "$<TARGET_FILE:naru_editor>"
      VERBATIM)
  else()
    message(WARNING "windeployqt를 찾지 못했습니다. Qt bin 폴더를 PATH에 추가해야 NaruStudio.exe가 실행됩니다.")
  endif()
endif()
