module;

#include <utility>
#include <array>
#include <string_view>
// #include <memory>
#include <vector>
#include <print>

export module edna.runtime.bytecode;

export import edna.runtime.value;

namespace Edna::Runtime {
    export enum class Opcode : std::uint8_t {
        nop,
        dup,
        push_null,
        push_bool,
        push_callee,
        // TODO: add 'push_self' opcode!
        push_global,
        push_const,
        get_local,
        set_local,
        get_prop,
        set_prop,
        pop,
        make_array,
        make_object,
        deref,
        negate_bool,
        negate_num,
        mod,
        mul,
        div,
        add,
        sub,
        compare_eq,
        compare_ne,
        compare_lt,
        compare_lte,
        compare_gt,
        compare_gte,
        test,
        jump,
        jump_back,
        jump_if,
        jump_else,
        call_ctor,
        call_fun,
        call_native,
        ret,
        // throw_obj,
        // catch_obj
        last
    };

    export struct Instruction {
        Opcode op;
        std::uint16_t arg;
    };

    export struct Chunk {
        std::vector<Value> consts;
        std::vector<Instruction> code;
    };

    export struct Program {
        ObjectHeap<Value> pre_heap;
        std::vector<Value> globals;
        std::vector<Chunk> chunks;
    };

    export void disassemble_program(const Program& program) {
        static constexpr std::array<std::string_view, static_cast<std::size_t>(Opcode::last)> opcode_names = {
            "nop",
            "dup",
            "push_null",
            "push_bool",
            "push_callee",
            "push_global",
            "push_const",
            "get_local",
            "set_local",
            "get_prop",
            "set_prop",
            "pop",
            "make_array",
            "make_object",
            "deref",
            "negate_bool",
            "negate_num",
            "mod",
            "mul",
            "div",
            "add",
            "sub",
            "compare_eq",
            "compare_ne",
            "compare_lt",
            "compare_lte",
            "compare_gt",
            "compare_gte",
            "test",
            "jump",
            "jump_back",
            "jump_if",
            "jump_else",
            "call_ctor",
            "call_fun",
            "call_native",
            "ret"
        };

        static constexpr std::array<std::string_view, static_cast<std::size_t>(ValueScalarHint::last)> constants_names = {
            "nan",
            "null",
            "boolean",
            "integer",
            "real",
            "local_id",
            "heap_id"
        };

        const auto& [program_heap, program_globals, program_chunk] = program;

        std::println("\x1b[1;33mProgram dump:\x1b[0m\n\n");

        const auto& [chunk_constants, chunk_code] = program_chunk.back();

        std::println("\x1b[1;33mMain Chunk:\x1b[0m\n");

        for (auto constant_id = 0; const auto& constant_v : chunk_constants) {
            std::println("constant {}:", constant_id);

            if (constant_v.hint() == ValueScalarHint::heap_id) {
                std::println("{}", program_heap.at(static_cast<int>(constant_v.scalar()))->as_str(nullptr));
            } else {
                display_value(constant_v);
            }

            constant_id++;
        }

        for (auto instruction_pos = 0; const auto& [instruction_op, instruction_arg] : chunk_code) {
            std::println("{}: {} {}", instruction_pos, opcode_names.at(std::to_underlying(instruction_op)), instruction_arg);
            instruction_pos++;
        }
    }
}
