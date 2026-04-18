module;

#include <utility>
// #include <stdexcept>
#include <memory>
#include <vector>
#include <stack>
// #include <algorithm>

export module edna.runtime.context;

export import edna.runtime.bytecode;

namespace Edna::Runtime {
    export enum class EvalStatus : std::uint8_t {
        pending,
        ok,
        error_memory,
        error_custom
    };

    export struct EvalFrame {
        const Instruction* caller_ret_ip;
        const Value* caller_cvp;
        std::uint32_t caller_bp; //? for local-stack only, not temps
        std::uint32_t callee_bp; //? for local-stack only, not temps
    };

    export struct EvalContext {
        ObjectHeap<Value> heap;
        std::stack<EvalFrame> frames;
        std::unique_ptr<Value[]> locals;
        std::unique_ptr<Value[]> stack;

        const Instruction* ip;  //? chunk bytecode ptr
        const Value* cvp;       //? chunk constant value ptr
        std::uint32_t bp;       //? stack & local base position of callee functions
        std::uint32_t sp;       //? stack top position
        EvalStatus status;      //? VM dispatch status

    private:
        EvalContext(Program& program, int local_capacity, int max_call_depth)
        : heap (std::move(program.pre_heap)), frames {}, locals {}, stack {}, ip {nullptr}, cvp {nullptr}, bp {0}, sp {0}, status {EvalStatus::pending} {
            locals = std::make_unique<Value[]>(static_cast<std::size_t>(local_capacity));
            stack = std::make_unique<Value[]>(static_cast<std::size_t>(local_capacity));

            const auto starting_chunk_id = program.entry_chunk_id;

            ip = program.chunks.at(starting_chunk_id).code.data();
            cvp = program.chunks.at(starting_chunk_id).consts.data();

            //? 1. 2 nulls should be pushed for slots 0 and 1 since the implicit main & its null `selfArg` are not explicitly usable!
            stack[sp] = Value::create_from_dud();
            sp++;
            stack[sp] = Value::create_from_dud();
            bp = sp;

            frames.emplace(nullptr, nullptr, bp, bp);
        }

    public:
        template <typename ... CArgs> requires (std::is_constructible_v<EvalContext, CArgs...>)
        [[nodiscard]] static constexpr EvalContext create(CArgs&& ... ctor_args) {
            return EvalContext(std::forward(ctor_args)...);
        }
    };
}
