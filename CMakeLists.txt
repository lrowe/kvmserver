cmake_minimum_required(VERSION 3.16)
project(kvmserver CXX)

add_subdirectory(ext/tinykvm tinykvm)
add_subdirectory(ext/json    json)

add_executable(kvmserver
	src/main.cpp
	src/config.cpp
	src/file.cpp
	src/warmup.cpp
	src/vm.cpp
)
target_compile_features(kvmserver PUBLIC cxx_std_20)
target_link_libraries(kvmserver
	tinykvm
	nlohmann_json
	jemalloc
)

if (SANITIZE)
	target_compile_options(kvmserver PRIVATE -fsanitize=address,undefined)
	target_link_options(kvmserver PRIVATE
		-fsanitize=address,undefined
		-fno-omit-frame-pointer
	)
endif()
