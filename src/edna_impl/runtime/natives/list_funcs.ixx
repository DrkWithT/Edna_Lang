module;

#include <utility>
#include <span>

export module edna.runtime.natives.list_funcs;

import edna.runtime.tables;

namespace Edna::Runtime::Natives {
    export [[nodiscard]] EvalStatus list_len(void* opaque, std::uint8_t argc) {
        auto context = reinterpret_cast<EvalContext*>(opaque);
        const std::uint32_t callee_bp = context->sp - argc;

        if (
            auto table_p = dynamic_cast<Table*>(context->heap.at(context->stack[callee_bp - 1].scalar()));
            table_p != nullptr
        ) {
            const auto table_len = static_cast<int>(table_p->indexables().size());

            context->stack[callee_bp - 1] = Value::create_from_int(table_len);
            context->sp = callee_bp - 1;

            return EvalStatus::pending;
        }

        return EvalStatus::bad_op_arg;
    }

    export [[nodiscard]] EvalStatus list_at(void* opaque, std::uint8_t argc) {
        auto context = reinterpret_cast<EvalContext*>(opaque);
        const std::uint32_t callee_bp = context->sp - argc;
        const std::span<Edna::Runtime::Value> arguments {
            context->stack.get() + callee_bp + 1,
            static_cast<std::uint32_t>(argc)
        };

        if (
            auto table_p = dynamic_cast<Table*>(
                context->heap.at(
                    context->stack[callee_bp - 1].scalar()
                )
            );
            table_p != nullptr
        ) {
            context->stack[callee_bp - 1] = table_p->get_property(context, arguments[0].scalar());
            context->sp = callee_bp - 1;

            return EvalStatus::pending;
        }

        return EvalStatus::bad_op_arg;
    }
}
