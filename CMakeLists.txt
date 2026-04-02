cmake_minimum_required(VERSION 4.2.0)

project(ednalang CXX)

add_library(edna_impl)
target_sources(
    edna_impl
    PUBLIC FILE_SET edna_impl_modules
    TYPE CXX_MODULES
    FILES src/edna_impl/library.ixx
    src/edna_impl/frontend/lexicals.ixx
    src/edna_impl/frontend/lexer.ixx
    src/edna_impl/frontend/ast.ixx
    src/edna_impl/frontend/parser.ixx
    # src/edna_impl/backend/compile_ctx.ixx
    # src/edna_impl/backend/compile_exprs.ixx
    # src/edna_impl/backend/compile_stmts.ixx
    # src/edna_impl/runtime/value.ixx
    # src/edna_impl/runtime/vm.ixx
)

add_executable(edna src/main.cpp)
target_link_directories(edna PUBLIC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} PUBLIC ${MY_LLVM_LIBCXX_DIR})
target_link_libraries(edna PUBLIC edna_impl)
message(NOTICE "Executable at: ./build/edna")

# enable_testing()
# add_subdirectory(tests)
