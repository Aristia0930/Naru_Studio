add_library(naru_engine STATIC
  core/Log.cpp
  core/Value.cpp
  events/EventRunner.cpp
  events/BuiltinCommands.cpp
  runtime/Session.cpp
)

target_include_directories(naru_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(naru_engine PUBLIC cxx_std_20)
