#include <string>
#include <string_view>
#include <sstream>
#include <fstream>
#include <chrono>
#include <print>

import edna_impl;

constexpr int edna_max_local_slots = 4096;

[[nodiscard]] std::string read_source_file(const std::string& source_path) {
    std::ifstream reader {source_path};

    if (!reader.is_open()) {
        return {""};
    }

    std::ostringstream sout;
    std::string temp_line;

    while (std::getline(reader, temp_line)) {
        sout << temp_line << '\n';
        temp_line.clear();
    }

    return sout.str();
}

// todo: refactor all setup + interpreter logic into a driver class later.
int main(int argc, char* argv[]) {
    using namespace Edna;

    if (argc < 2 || argc > 3) {
        std::println("Usage:\nedna [info | dump | run] <args...>\n\tinfo: print info\n\tdump: print bytecode dump only\n\trun: run without bytecode dump\n");
        return 1;
    }

    std::string_view arg_1 = argv[1];
    std::string arg_2 {};

    if (arg_1 == "info") {
        std::println("Usage:\nedna [info | dump | run] <args...>\n\tinfo: print info\n\tdump: print bytecode dump only\n\trun: run without bytecode dump\n");
        return 0;
    }
    
    if ((arg_1 != "run" && arg_1 != "dump")) {
        std::println("Usage:\nedna [info | dump | run] <args...>\n\tinfo: print info\n\tdump: print bytecode dump only\n\trun: run without bytecode dump\n");
        return 1;
    }

    if (argc == 3) {
        arg_2 = argv[2];
    } else {
        std::println("Missing source file argument.");
        return 1;
    }

    auto source_string = read_source_file(arg_2);

    Frontend::Lexer lexer;

    lexer.add_edna_lexical("null", Frontend::TokenTag::literal_null);
    lexer.add_edna_lexical("true", Frontend::TokenTag::literal_true);
    lexer.add_edna_lexical("false", Frontend::TokenTag::literal_false);
    lexer.add_edna_lexical("self", Frontend::TokenTag::keyword_self);
    lexer.add_edna_lexical("fun", Frontend::TokenTag::keyword_fun);
    lexer.add_edna_lexical("uses", Frontend::TokenTag::keyword_uses);
    lexer.add_edna_lexical("let", Frontend::TokenTag::keyword_let);
    lexer.add_edna_lexical("mut", Frontend::TokenTag::keyword_mut);
    lexer.add_edna_lexical("cond", Frontend::TokenTag::keyword_cond);
    lexer.add_edna_lexical("case", Frontend::TokenTag::keyword_case);
    lexer.add_edna_lexical("else", Frontend::TokenTag::keyword_else);
    lexer.add_edna_lexical("symbol", Frontend::TokenTag::keyword_symbol);
    lexer.add_edna_lexical("prec", Frontend::TokenTag::keyword_prec);
    lexer.add_edna_lexical("-", Frontend::TokenTag::op_neg);
    lexer.add_edna_lexical("!", Frontend::TokenTag::op_bang);
    lexer.add_edna_lexical("%", Frontend::TokenTag::op_mod);
    lexer.add_edna_lexical("*", Frontend::TokenTag::op_mult);
    lexer.add_edna_lexical("/", Frontend::TokenTag::op_div);
    lexer.add_edna_lexical("+", Frontend::TokenTag::op_plus);
    lexer.add_edna_lexical("-", Frontend::TokenTag::op_sub);
    lexer.add_edna_lexical("==", Frontend::TokenTag::op_equals);
    lexer.add_edna_lexical("!=", Frontend::TokenTag::op_unequal);
    lexer.add_edna_lexical("<", Frontend::TokenTag::op_lesser);
    lexer.add_edna_lexical(">", Frontend::TokenTag::op_greater);
    lexer.add_edna_lexical("<=", Frontend::TokenTag::op_lte);
    lexer.add_edna_lexical(">=", Frontend::TokenTag::op_gte);
    lexer.add_edna_lexical("&&", Frontend::TokenTag::op_and);
    lexer.add_edna_lexical("||", Frontend::TokenTag::op_or);
    lexer.add_edna_lexical("=", Frontend::TokenTag::op_assign);
    lexer.add_edna_lexical("=>", Frontend::TokenTag::arrow);
    lexer.add_edna_lexical(".", Frontend::TokenTag::dot);
    lexer.add_edna_lexical("...", Frontend::TokenTag::ellipses);

    std::string_view source_view {source_string};
    Frontend::Token temp;

    lexer.use_source(source_view);

    Frontend::Parser parser {lexer, source_view};

    auto ast_decls = parser(lexer, source_view);

    if (ast_decls.empty()) {
        return 1;
    }

    Compile::CompileContext compiler_state;

    compiler_state.add_expr_emitter(Frontend::ExprTag::atom, std::make_unique<Compile::AtomEmitter>());
    compiler_state.add_expr_emitter(Frontend::ExprTag::cond, std::make_unique<Compile::CondEmitter>());
    compiler_state.add_expr_emitter(Frontend::ExprTag::block, std::make_unique<Compile::BlockEmitter>());
    compiler_state.add_expr_emitter(Frontend::ExprTag::array, std::make_unique<Compile::ArrayLiteralEmitter>());
    compiler_state.add_expr_emitter(Frontend::ExprTag::lambda, std::make_unique<Compile::LambdaEmitter>());
    compiler_state.add_expr_emitter(Frontend::ExprTag::lhs, std::make_unique<Compile::LhsEmitter>());
    compiler_state.add_expr_emitter(Frontend::ExprTag::call, std::make_unique<Compile::CallEmitter>());
    compiler_state.add_expr_emitter(Frontend::ExprTag::unary, std::make_unique<Compile::UnaryEmitter>());
    compiler_state.add_expr_emitter(Frontend::ExprTag::binary, std::make_unique<Compile::BinaryEmitter>());
    compiler_state.add_expr_emitter(Frontend::ExprTag::assign, std::make_unique<Compile::AssignEmitter>());

    compiler_state.add_stmt_emitter(Frontend::StmtTag::vars, std::make_unique<Compile::VarsEmitter>());
    compiler_state.add_stmt_emitter(Frontend::StmtTag::expr_stmt, std::make_unique<Compile::ExprStmtEmitter>());

    auto program_opt = Compile::compile_all(compiler_state, ast_decls, source_string);

    if (!program_opt) {
        return 1;
    } else if (arg_1 == "dump") {
        Runtime::disassemble_program(program_opt.value());
        return 0;
    }

    Runtime::VM vm (
        Runtime::EvalContext {
            program_opt.value(),
            edna_max_local_slots
        }
    );

    auto run_begin = std::chrono::steady_clock::now();
    auto status = vm.template run<Runtime::Handlers>();
    auto running_time = std::chrono::steady_clock::now() - run_begin;

    std::println("Runtime: \x1b[1;33m{}\x1b[0m ms", std::chrono::duration_cast<std::chrono::milliseconds>(running_time));

    if (status != Runtime::EvalStatus::ok) {
        return 1;
    }

    Runtime::display_value(vm.result());
}
