cmake_minimum_required(VERSION 3.15)
project(bullet_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bullet_engine
    src/engine.cpp
    src/state_utils.cpp
)
target_include_directories(bullet_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(engine_tests
    tests/test_engine.cpp
)
target_link_libraries(engine_tests PRIVATE bullet_engine)

# Optional: Emscripten WebAssembly target (build only when using emscripten toolchain)
if (EMSCRIPTEN)
  add_executable(bullet_engine_wasm
      src/engine.cpp
      src/state_utils.cpp
      src/wasm_bridge.cpp
  )
  target_include_directories(bullet_engine_wasm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  # Suggested link flags (adjust as needed)
  target_link_options(bullet_engine_wasm PRIVATE
      "-sMODULARIZE=1"
      "-sENVIRONMENT=web"
      "-sALLOW_MEMORY_GROWTH=1"
      "-sEXPORT_ES6=1"
  )
endif()
