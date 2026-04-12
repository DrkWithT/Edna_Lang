module;

#include <type_traits>
#include <concepts>
#include <vector>
#include <stack>

export module edna.runtime.meta;

namespace Edna::Runtime {    
    export class Value; //? SEE: runtime/value.ixx
    export class ObjectBase; //? SEE: runtime/objects.ixx
    export class ObjectHeap; //? SEE: runtime/heap.ixx
    
    export struct Instruction; //? SEE: runtime/bytecode.ixx
    export enum class EvalStatus : std::uint8_t; //? SEE: runtime/context.ixx
    export struct EvalFrame; //? SEE: runtime/context.ixx

    export namespace Meta {
        template <typename C>
        concept ContextKind = requires (C arg) {
            {auto(C::create())} -> std::same_as<C>;
            {auto(arg.ip)} -> std::same_as<const Instruction*>; //? represents instruction pointer into chunk code
            {auto(arg.cvp)} -> std::same_as<const Value*>; //? represents pointer into chunk constants
            {auto(arg.bp)} -> std::same_as<int>;
            {auto(arg.sp)} -> std::same_as<int>;
            {auto(arg.status)} -> std::same_as<EvalStatus>;
            {auto(arg.stack)} -> std::same_as<std::vector<Value>>;
            {auto(arg.locals)} -> std::same_as<std::vector<Value>>;
            {auto(arg.frames)} -> std::same_as<std::stack<EvalFrame>>
            {auto(arg.heap)} -> std::same_as<ObjectHeap>;
        };
    }
}
