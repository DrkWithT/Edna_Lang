module;

#include <utility>
#include <memory>

export module edna.runtime.vm;

import edna.runtime.context;

namespace Edna::Runtime {
    export class VM {
    private:
        EvalContext m_context;

    public:
        constexpr VM(EvalContext ctx) noexcept
        : m_context (std::move(ctx)) {}

        [[nodiscard]] constexpr const Value& result() const noexcept {
            return m_context.stack[0];
        }

        const EvalContext& context() const noexcept {
            return m_context;
        }

        //? NOTE 1: Handlers must be a struct providing decoupled opcode handlers for the bytecode VM- All its methods must be static and take some specific parameter types. These types must be an `EvalContext&` besides `Value*` for the local-values and stack.
        //? NOTE 2: these handlers must be TCO (tail-call dispatched).
        template <typename Handlers> requires requires (Handlers h) { {Handlers::dispatch}; }
        [[nodiscard]] constexpr EvalStatus run() {
            return Handlers::dispatch(m_context, m_context.ip, m_context.cvp, m_context.stack.get());
        }
    };
}