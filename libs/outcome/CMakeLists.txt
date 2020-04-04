add_library(outcome INTERFACE)
target_sources(outcome INTERFACE include/outcome.hpp)
target_include_directories(outcome INTERFACE include)
add_library(outcome::outcome ALIAS outcome)
