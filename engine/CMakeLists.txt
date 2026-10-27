add_library(naru_engine STATIC
  core/Log.cpp
  core/Value.cpp
  data/Database.cpp
  events/EventRunner.cpp
  events/BuiltinCommands.cpp
  events/EventCompiler.cpp
  events/EventData.cpp
  project/Project.cpp
  project/ProjectIO.cpp
  runtime/GameWorld.cpp
  runtime/Input.cpp
  runtime/Session.cpp
)

target_include_directories(naru_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# JSON은 엔진 내부 구현에만 사용한다. 공개 헤더에는 노출하지 않는다.
target_include_directories(naru_engine SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
target_compile_features(naru_engine PUBLIC cxx_std_20)
