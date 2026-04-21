module;

export module edna.runtime.meta;

export import edna.runtime.context;

namespace Edna::Runtime {
    export template <typename HandlersProvider>
    using vm_opcode_handler = EvalStatus(HandlersProvider::*)(EvalContext&, const Value*, Value*, Value*);
}