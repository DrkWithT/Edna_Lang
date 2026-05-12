module;

#include <utility>
#include <vector>
#include <algorithm>

export module edna.runtime.natives.list_funcs;

import edna.runtime.tables;

namespace Edna::Runtime::Natives {
    export [[nodiscard]] EvalStatus list_len(void* opaque, std::uint8_t argc) {
        auto context = reinterpret_cast<EvalContext*>(opaque);
        const std::uint32_t callee_bp = context->sp - argc;

        if (
            auto table_p = static_cast<Table*>(context->heap.at(context->stack[callee_bp - 1].scalar()));
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

        if (
            auto table_p = static_cast<Table*>(
                context->heap.at(
                    context->stack[callee_bp - 1].scalar()
                )
            );
            table_p != nullptr
        ) {
            context->stack[callee_bp - 1] = table_p->get_property(context, context->stack[callee_bp + 1].scalar());
            context->sp = callee_bp - 1;

            return EvalStatus::pending;
        }

        return EvalStatus::bad_op_arg;
    }

    export [[nodiscard]] EvalStatus list_find(void* opaque, std::uint8_t argc) {
        auto context = reinterpret_cast<EvalContext*>(opaque);
        const std::uint32_t callee_bp = context->sp - argc;

        const auto list_p = dynamic_cast<const Table*>(context->heap.at(context->stack[callee_bp - 1].scalar()));

        if (list_p == nullptr) {
            return EvalStatus::bad_op_arg;
        }

        if (const Value& target_arg = context->stack[callee_bp + 1], reversed_arg = context->stack[callee_bp + 2]; target_arg.hint() != ValueScalarHint::heap_id) {
            const auto& list_items = list_p->indexables();
            std::int32_t result = -1;

            for (std::int32_t i = 0; const auto& item : list_items) {
                if (!item.is_nan() && !target_arg.is_nan()) {
                    result = (item.as_double() == target_arg.as_double()) ? i : -1;
                } else if (item.hint() == target_arg.hint()) {
                    switch (item.hint()) {
                    case ValueScalarHint::null:
                        result = i;
                        break;
                    case ValueScalarHint::boolean:
                    case ValueScalarHint::integer:
                    case ValueScalarHint::heap_id:
                        result = (item.scalar() == target_arg.scalar()) ? i : -1;
                        break;
                    default:
                        break;
                    }
                }

                if (result != -1) {
                    break;
                }

                i++;
            }

            context->stack[callee_bp - 1] = Value::create_from_int(result);
            context->sp = callee_bp - 1;

            return EvalStatus::pending;
        } else {
            context->stack[callee_bp - 1] = Value::create_from_int(-1);
            context->sp = callee_bp - 1;

            return EvalStatus::pending;
        }
    }

    export [[nodiscard]] EvalStatus list_push(void* opaque, std::uint8_t argc) {
        auto context = reinterpret_cast<EvalContext*>(opaque);
        const std::uint32_t callee_bp = context->sp - argc;
        const std::uint32_t arg_offset_end = 1 + argc;

        auto list_p = dynamic_cast<Table*>(context->heap.at(context->stack[callee_bp - 1].scalar()));

        if (list_p == nullptr) {
            return EvalStatus::bad_op_arg;
        }

        for (std::uint32_t i = 1; i < arg_offset_end; i++) {
            list_p->indexables().push_back(context->stack[callee_bp + i]);
        }

        context->sp = callee_bp - 1; // return self

        return EvalStatus::pending;
    }

    export [[nodiscard]] EvalStatus list_pop(void* opaque, std::uint8_t argc) {
        auto context = reinterpret_cast<EvalContext*>(opaque);
        const std::uint32_t callee_bp = context->sp - argc;
        const auto pop_count = ([] (const Value& pop_count_v, std::uint8_t arg_count) constexpr noexcept -> int {
            if (arg_count == 0) {
                return 1;
            } else if (pop_count_v.hint() == ValueScalarHint::integer) {
                return pop_count_v.scalar();
            } else {
                return -1;
            }
        })(context->stack[callee_bp + 1], argc);

        if (pop_count == -1) {
            return EvalStatus::bad_op_arg;
        }

        auto list_p = dynamic_cast<Table*>(context->heap.at(context->stack[callee_bp - 1].scalar()));

        if (list_p == nullptr) {
            return EvalStatus::bad_op_arg;
        }

        auto& items = list_p->indexables();

        for (int pop_i = pop_count; pop_i > 0 && !items.empty(); pop_i--) {
            items.pop_back();
        }

        context->sp = callee_bp - 1; // return self

        return EvalStatus::pending;
    }

    export [[nodiscard]] EvalStatus list_rev(void* opaque, std::uint8_t argc) {
        auto context = reinterpret_cast<EvalContext*>(opaque);
        const std::uint32_t callee_bp = context->sp - argc;

        auto list_p = dynamic_cast<Table*>(context->heap.at(context->stack[callee_bp - 1].scalar()));

        if (list_p == nullptr) {
            return EvalStatus::bad_op_arg;
        }

        std::ranges::reverse(list_p->indexables());

        context->sp = callee_bp - 1; // return self

        return EvalStatus::pending;
    }
}
