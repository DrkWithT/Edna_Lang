#include <string>
#include <string_view>
#include <sstream>
#include <fstream>
#include <iostream>
#include <print>

import edna_impl;

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

int main(int argc, char* argv[]) {
    using namespace Edna;

    if (argc < 2 || argc > 3) {
        std::println("Usage:\nedna [info | dump | run] <args...>\n\tinfo: print info\n\tdump: print bytecode dump only\n\trun: run without bytecode dump\n");
        return 1;
    }

    std::string arg_2 {};

    if (std::string_view arg_1 = argv[1]; arg_1 == "info") {
        std::println("Usage:\nedna [info | dump | run] <args...>\n\tinfo: print info\n\tdump: print bytecode dump only\n\trun: run without bytecode dump\n");
        return 0;
    } else if (arg_1 == "run" && argc == 3) {
        arg_2 = argv[2];
    } else {
        std::println("Usage:\nedna [info | dump | run] <args...>\n\tinfo: print info\n\tdump: print bytecode dump only\n\trun: run without bytecode dump\n");
        return 1;
    }

    auto source_string = read_source_file(arg_2);

    Frontend::Lexer lexer;

    lexer.add_edna_lexical("null", Frontend::TokenTag::keyword_null);
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

    do {
        temp = lexer(source_view);

        if (temp.tag == Frontend::TokenTag::unknown) {
            std::println(std::cerr, "At source [{} {}]: unknown token of '{}' found!\n", temp.line, temp.col, temp.as_str_from(source_view));
            return 1;
        }
    } while (temp.tag != Frontend::TokenTag::eof);
}
