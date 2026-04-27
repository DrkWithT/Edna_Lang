module;

#include <utility>
#include <string_view>
#include <vector>
#include <flat_map>
#include <algorithm>

export module edna.compile.passes;

export import edna.compile.optimizer;

namespace Edna::Compile {
    export class TrimNOPs : public PassBase {
    private:
        std::vector<int> m_jump_positions {};

        [[nodiscard]] long sweep(std::vector<Runtime::Instruction>& chunk_code) {
            constexpr auto dud_position = -1;
            auto nops_begin = dud_position;
            auto nops_end = 0;
            auto nop_count = 0;
            auto score = 0;
            
            // * Step 1: record jump instruction positions before a NOP cluster, if any. The cluster's location is also recorded for book-keeping.
            for (auto instruction_idx = 0; const auto& [opcode, arg] : chunk_code) {
                if (opcode == Runtime::Opcode::nop) {
                    nops_begin = instruction_idx;
                    break;
                } else if (opcode == Runtime::Opcode::jump || opcode == Runtime::Opcode::jump_back || opcode == Runtime::Opcode::jump_else) {
                    m_jump_positions.emplace_back(instruction_idx);
                }

                instruction_idx++;
            }

            if (nops_begin == dud_position) {
                return score;
            }
            
            // * Step 2: upon a NOP cluster, count each NOP. This is how much to decrease any jump offsets passing the NOP cluster.
            nop_count = 1;

            for (auto nop_index = nops_begin + 1; ; nop_index++) {
                if (const auto& [opcode, arg] = chunk_code.at(nop_index); opcode == Runtime::Opcode::nop) {
                    nop_count++;
                } else {
                    break;
                }
            }

            nops_end = nops_begin + nop_count;

            // * Step 3: AFTER removing the NOP cluster, apply the most recent jump offset change to jumps passing the cluster's end.
            chunk_code.erase(chunk_code.begin() + nops_begin, chunk_code.begin() + nops_end);
            score = nop_count;

            for (const auto jump_op_pos : m_jump_positions) {
                if (auto& [opcode, arg] = chunk_code.at(jump_op_pos); jump_op_pos + arg >= nops_end) {
                    arg -= nop_count;
                } else if (const auto jump_target_pos = jump_op_pos + arg; jump_target_pos >= nops_begin && jump_target_pos < nops_end) {
                    arg -= jump_target_pos - nops_begin;
                }
            }

            return score;
        }

        void reset_state() {
            m_jump_positions.clear();
        }

    public:
        constexpr TrimNOPs() noexcept = default;

        [[nodiscard]] std::string_view name() const noexcept override {
            return "Trim NOPs";
        }

        [[nodiscard]] long apply(std::vector<Runtime::Instruction>& chunk_code) override {
            const auto removal_score = sweep(chunk_code);

            reset_state();

            return removal_score;
        }
    };

    export class CondenseOps : public PassBase {
    private:
        std::flat_map<std::size_t, Runtime::Opcode> items;

        void map_tri_pattern(Runtime::Opcode op0, Runtime::Opcode op1, Runtime::Opcode op2, Runtime::Opcode super_op) {
            // ? NOTE: create the triple-opcode hash of the pattern in reverse, since patterns are kept in a deque and accumulated in LIFO manner. Excess elements over 3 flush out the oldest of 3 opcodes in the window.
            const auto key_hash = static_cast<std::size_t>(op0) + static_cast<std::size_t>(op1) + static_cast<std::size_t>(op2);

            items[key_hash] = super_op;
        }

        [[nodiscard]] bool try_fix_tri_pattern(Runtime::Opcode op0, Runtime::Opcode op1, Runtime::Opcode op2, std::vector<Runtime::Instruction>& chunk_code, std::size_t pattern_begin) noexcept {
            if (auto super_opcode_it = items.find(
                static_cast<std::size_t>(op0)
                + static_cast<std::size_t>(op1)
                + static_cast<std::size_t>(op2)
            ); super_opcode_it != items.end()) {
                switch (
                    const auto super_op = super_opcode_it->second;
                    super_op
                ) {
                case Runtime::Opcode::padd_lk: {
                    // TODO: refactor the pattern replacers to map to non-capturing lambdas.
                    if (op0 == Runtime::Opcode::get_local && op1 == Runtime::Opcode::push_const) {
                        const std::uint16_t encoded_args = static_cast<std::uint16_t>(chunk_code[pattern_begin].arg) + static_cast<std::uint16_t>(chunk_code[pattern_begin + 1].arg << 8);

                        chunk_code[pattern_begin] = Runtime::Instruction {
                            .op = super_op,
                            // ? NOTE: Encode the local & constant IDs as 1 unsigned short.
                            .arg = encoded_args
                        };
                        chunk_code[pattern_begin + 1] = Runtime::Instruction {
                            .op = Runtime::Opcode::nop,
                            .arg = 0
                        };
                        chunk_code[pattern_begin + 2] = Runtime::Instruction {
                            .op = Runtime::Opcode::nop,
                            .arg = 0
                        };
                        return true;
                    } else if (op0 == Runtime::Opcode::push_const && op1 == Runtime::Opcode::get_local) {
                        const std::uint16_t encoded_args = static_cast<std::uint16_t>(chunk_code[pattern_begin + 1].arg) + static_cast<std::uint16_t>(chunk_code[pattern_begin].arg << 8);

                        chunk_code[pattern_begin] = Runtime::Instruction {
                            .op = super_op,
                            // ? NOTE: Local ID still fills the LSB, big-endian style!
                            .arg = encoded_args
                        };
                        chunk_code[pattern_begin + 1] = Runtime::Instruction {
                            .op = Runtime::Opcode::nop,
                            .arg = 0
                        };
                        chunk_code[pattern_begin + 2] = Runtime::Instruction {
                            .op = Runtime::Opcode::nop,
                            .arg = 0
                        };
                        return true;
                    }
                }
                case Runtime::Opcode::psub_lk: {
                    if (op0 == Runtime::Opcode::get_local && op1 == Runtime::Opcode::push_const) {
                        const std::uint16_t encoded_args = static_cast<std::uint16_t>(chunk_code[pattern_begin].arg) + static_cast<std::uint16_t>(chunk_code[pattern_begin + 1].arg << 8);

                        chunk_code[pattern_begin] = Runtime::Instruction {
                            .op = super_op,
                            .arg = encoded_args
                        };
                        chunk_code[pattern_begin + 1] = Runtime::Instruction {
                            .op = Runtime::Opcode::nop,
                            .arg = 0
                        };
                        chunk_code[pattern_begin + 2] = Runtime::Instruction {
                            .op = Runtime::Opcode::nop,
                            .arg = 0
                        };
                        return true;
                    }
                }
                default: break;
                }
            }

            return false;
        }

        [[nodiscard]] static constexpr Runtime::Opcode peek_opcode_by(const std::vector<Runtime::Instruction>& chunk_code, std::size_t peek_pos) noexcept {
            if (peek_pos >= chunk_code.size()) {
                return {};
            }

            return chunk_code[peek_pos].op;
        }

        // ! Example of super instruction replacement below:
        // * GET_FAST 0 (x), GET_CONST 0 (2), SUB <-- find patterns like this
        // * PSUM_LC 0 0, NOP, NOP
        // * (clean NOPs via TrimNOPs pass...)
        [[nodiscard]] long sweep(std::vector<Runtime::Instruction>& chunk_code) {
            constexpr std::size_t tri_pattern_n = 3;

            const auto code_length = chunk_code.size();
            long replacements = 0;

            for (long opcode_pos = 0; auto& [op, arg] : chunk_code) {
                if (
                    const auto op0 = peek_opcode_by(chunk_code, opcode_pos), op1 = peek_opcode_by(chunk_code, opcode_pos + 1), op2 = peek_opcode_by(chunk_code, opcode_pos + 2);
                    try_fix_tri_pattern(op0, op1, op2, chunk_code, opcode_pos)
                ) {
                    replacements++;
                }

                opcode_pos += 3;
            }

            return replacements;
        }

    public:
        constexpr CondenseOps() noexcept
        : items {} {
            map_tri_pattern(Runtime::Opcode::get_local, Runtime::Opcode::push_const, Runtime::Opcode::add, Runtime::Opcode::padd_lk);
            map_tri_pattern(Runtime::Opcode::push_const, Runtime::Opcode::get_local, Runtime::Opcode::add, Runtime::Opcode::padd_lk);
            map_tri_pattern(Runtime::Opcode::get_local, Runtime::Opcode::push_const, Runtime::Opcode::sub, Runtime::Opcode::psub_lk);
        }

        [[nodiscard]] std::string_view name() const noexcept override {
            return "Condense Opcodes";
        }

        [[nodiscard]] long apply(std::vector<Runtime::Instruction>& chunk_code) override {            
            return sweep(chunk_code);
        }
    };
}
