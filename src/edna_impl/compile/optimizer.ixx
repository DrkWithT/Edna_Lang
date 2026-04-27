module;

#include <utility>
#include <type_traits>
#include <memory>
#include <string_view>
#include <array>
#include <vector>
#include <queue>
#include <algorithm>

export module edna.compile.optimizer;

export import edna.runtime.bytecode;

namespace Edna::Compile {
    export enum class PassTag : std::uint8_t {
        condense_ops,
        trim_nops,
        last
    };

    /**
     * @brief Provides an interface for bytecode transformations which can remove / change generated instructions to optimize an Edna program:
     * 1. Super-instruction replacement?
     * 2. Dead code removal / NOP removal?
     */
    export class PassBase {
    public:
        virtual ~PassBase() = default;

        virtual std::string_view name() const noexcept = 0;

        // ? NOTE: Returns a heuristic score as a signed long for the optimizer to determine when there's no significant improvement. Maybe this should reference a Chunk of bytecode for more information, especially constants.
        virtual long apply(std::vector<Runtime::Instruction>& chunk_code) = 0;
    };

    export struct Optimizer {
    private:
        static constexpr long dud_score = 100;

        std::array<std::unique_ptr<PassBase>, static_cast<std::size_t>(PassTag::last)> m_passes;
        std::queue<PassTag> m_schedule;
        long m_previous_score;
        long m_current_score;

    public:
        Optimizer() noexcept
        : m_passes {}, m_schedule {}, m_previous_score {dud_score}, m_current_score {dud_score} {
            m_schedule.emplace(PassTag::condense_ops);
            m_schedule.emplace(PassTag::trim_nops);
        }

        // ? NOTE: All optimizer passes don't need complex initialization, as they only need a bytecode blob to visit.
        void add_pass(PassTag tag, std::unique_ptr<PassBase> pass) noexcept {
            m_passes[static_cast<std::size_t>(tag)] = std::move(pass);
        }

        [[nodiscard]] constexpr bool is_done() const noexcept {
            return m_previous_score + m_current_score == 0;
        }

        // ? NOTE: Returns a heuristic score as a signed long for the optimizer to determine when there's no more improvements.
        // ? This could do something similar to hill-climbing: Suppose we have a round-robin scheduling of the 2 passes, terminating at L1 + L0 == 0 eventually (the local minima / maxima). Then something like this happens:
        // * Sample bytecode: PUSH_CONST 0, GET_LOCAL 0, ADD, PUSH_LOCAL 0, MUL
        // * Assuming a heuristic of instructions removed.
        // * Opcode condensing first gives PSUM_LC 0 0, NOP, NOP, PUSH_LOCAL 0, MUL... Condense Local0 + Const0 --> H = 2
        // * NOP remover removes 2 NOPs: H = 2
        // * Opcode condensing runs again: PSUM_LC 0 0, MUL_LOCAL 0, NOP: H = 1
        // * NOP remover removes 1 NOP: H = 1
        // * Opcode condensing & NOP trimming find no common pattern & do nothing: H = 0, optimizations finished.
        void apply(std::vector<Runtime::Instruction>& chunk_code) {
            while (!is_done()) {
                m_previous_score = m_current_score;

                const auto next_pass_id = m_schedule.front();
                m_schedule.pop();
                
                m_current_score = m_passes.at(static_cast<std::size_t>(next_pass_id))->apply(chunk_code);
                
                m_schedule.push(next_pass_id);
            }

            // ? NOTE: Reset default schedule in case the loop ends mid-cycle.
            {
                std::queue<PassTag> dud {};
                m_schedule.swap(dud);
            }

            m_schedule.emplace(PassTag::condense_ops);
            m_schedule.emplace(PassTag::trim_nops);
            m_previous_score = dud_score;
            m_current_score = dud_score;
        }
    };
}