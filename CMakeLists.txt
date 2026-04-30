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
    src/edna_impl/compile/context.ixx
    src/edna_impl/compile/expr_emit.ixx
    src/edna_impl/compile/stmt_emit.ixx
    src/edna_impl/compile/optimizer.ixx
    src/edna_impl/compile/passes.ixx
    src/edna_impl/runtime/objects.ixx
    src/edna_impl/runtime/value.ixx
    src/edna_impl/runtime/bytecode.ixx
    src/edna_impl/runtime/context.ixx
    src/edna_impl/runtime/meta.ixx
    src/edna_impl/runtime/callables.ixx
    src/edna_impl/runtime/tables.ixx
    src/edna_impl/runtime/handlers1.ixx
    src/edna_impl/runtime/vm.ixx
)

add_executable(edna src/main.cpp)
target_link_directories(edna PUBLIC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} PUBLIC ${MY_LLVM_LIBCXX_DIR})
target_link_libraries(edna PUBLIC edna_impl)
target_compile_options(edna PRIVATE "-foptimize-sibling-calls")
message(NOTICE "Executable at: ./build/edna")

# enable_testing()
# add_subdirectory(tests)
