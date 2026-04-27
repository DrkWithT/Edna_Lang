module;

#include <utility>
#include <string_view>
#include <stack>
#include <vector>
#include <algorithm>

export module edna.compile.passes;

export import edna.compile.optimizer;

namespace Edna::Compile {
    // TODO 1: make NOP trimmer:
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

        [[nodiscard]] std::string_view name() const noexcept {
            return "Trim NOPs";
        }

        [[nodiscard]] long apply(std::vector<Runtime::Instruction>& chunk_code) {
            const auto removal_score = sweep(chunk_code);

            reset_state();

            return removal_score;
        }
    };

    export class CondenseOps : public PassBase {
    private:
        // todo

    public:
        constexpr CondenseOps() noexcept = default; // todo: implememt internal state if needed.

        [[nodiscard]] std::string_view name() const noexcept {
            return "Condense Opcodes";
        }

        [[nodiscard]] long apply(std::vector<Runtime::Instruction>& chunk_code) {
            return 0;
        }
    };
}
