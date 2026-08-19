find_package(Boost REQUIRED COMPONENTS multi_index uuid compute variant)
find_path(TSL_ORDERED_MAP_INCLUDE_DIRS "tsl/ordered_hash.h")

add_subdirectory(core)
add_subdirectory(python)
add_subdirectory(gui)