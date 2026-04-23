module;

#include <utility>
#include <memory>

export module edna.runtime.context;

export import edna.runtime.bytecode;

namespace Edna::Runtime {
    export enum class EvalStatus : std::uint8_t {
        pending,
        ok,
        memory,
        bad_op_arg,
        unsupported_op
    };

    export struct EvalContext {
        ObjectHeap<Value> heap;
        std::unique_ptr<Value[]> stack;

        const Instruction* ip;  //? chunk bytecode ptr
        const Value* cvp;       //? chunk constant value ptr
        std::uint32_t bp;       //? stack & local base position of callee functions
        std::uint32_t sp;       //? stack top position
        std::uint16_t depth;    //? Call depth (hardcoded for now)
        EvalStatus status;      //? VM dispatch status

        EvalContext(Program& program, int local_capacity)
        : heap (std::move(program.pre_heap)), stack {}, ip {program.chunks.back().code.data()}, cvp {program.chunks.back().consts.data()}, bp {1}, sp {1}, depth {1}, status {EvalStatus::pending} {
            stack = std::make_unique<Value[]>(static_cast<std::size_t>(local_capacity));

            //? 1. 2 nulls should be pushed for slots 0 and 1 since the implicit main & its null `selfArg` are not explicitly usable!
            stack[0] = Value::create_from_dud();
            stack[1] = Value::create_from_dud();
        }
    };
}
