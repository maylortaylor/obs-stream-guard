# Third party code, pinned to exact versions.
include_guard(GLOBAL)
include(FetchContent)

FetchContent_Declare(
  vibra
  GIT_REPOSITORY https://github.com/BayernMuller/vibra.git
  GIT_TAG 59cfe94a6408c7d5642cc6c617044c3176bf3498
  # Skip vibra's own CMake project. Only its library sources are built, below.
  SOURCE_SUBDIR
  include
)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(vibra nlohmann_json)

add_library(
  sg_vibra
  STATIC
  ${vibra_SOURCE_DIR}/lib/vibra.cpp
  ${vibra_SOURCE_DIR}/lib/algorithm/signature.cpp
  ${vibra_SOURCE_DIR}/lib/algorithm/frequency.cpp
  ${vibra_SOURCE_DIR}/lib/algorithm/signature_generator.cpp
  ${vibra_SOURCE_DIR}/lib/audio/wav.cpp
  ${vibra_SOURCE_DIR}/lib/audio/downsampler.cpp
  ${vibra_SOURCE_DIR}/third-party/kissfft/kiss_fft.c
  ${vibra_SOURCE_DIR}/third-party/kissfft/kiss_fftr.c
)
target_include_directories(
  sg_vibra
  SYSTEM
  PUBLIC ${vibra_SOURCE_DIR}/include
  PRIVATE ${vibra_SOURCE_DIR}/lib ${vibra_SOURCE_DIR}/third-party/kissfft
)
target_compile_definitions(sg_vibra PRIVATE kiss_fft_scalar=double _USE_MATH_DEFINES)
set_target_properties(sg_vibra PROPERTIES POSITION_INDEPENDENT_CODE ON COMPILE_WARNING_AS_ERROR OFF)
# The warnings come from third party code.
target_compile_options(sg_vibra PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
