module;

#include <cstdint>
#include <memory>
#include <vector>
#include <variant>

export module edna.frontend.ast;

import edna.frontend.lexicals;

namespace Edna::Frontend {
    export enum class ExprTag : std::uint8_t {
        atom, //? NOTE: contains a simple literal expr
        array,
        lambda,
        block,
        cond,
        lhs,
        call,
        unary,
        binary,
        assign,
        last
    };

    export enum class StmtTag : std::uint8_t {
        var_decl,
        symbol_def, //? NOTE: custom operator
        expr_stmt,
        function_decl,
        top_level,
        last
    };

    export enum class AstOp : std::uint8_t {
        ast_noop,
        ast_dot_access,
        ast_index_access,
        ast_neg,
        ast_bang,
        ast_mod,
        ast_mult,
        ast_div,
        ast_plus,
        ast_sub,
        ast_equals,
        ast_unequal,
        ast_lesser,
        ast_greater,
        ast_lte,
        ast_gte,
        ast_and,
        ast_or,
        ast_assign,
        ast_other,
        last
    };

    export enum class ParamTag : std::uint8_t {
        param_fun,
        param_ltr,
        param_opr,
        param_expr,
        last
    };

    export template <typename ... Tp>
    struct Stmt {
        std::variant<Tp ...> data;
        int line;
        bool trailing; //? NOTE: for returns
        StmtTag tag;
    };

    export struct FunctionDecl;
    export struct VarDecl;
    export struct Vars;
    export struct SymbolDef;
    export struct ExprStmt;

    export using StmtNode = Stmt<FunctionDecl, Vars, SymbolDef, ExprStmt>;
    export using StmtPtr = std::unique_ptr<StmtNode>;

    export struct Param {
        Token token;
        bool is_pack;
    };

    export template <typename ... Tp>
    struct Expr {
        std::variant<Tp ...> data;
        int line;
        ExprTag tag;
    };

    export struct Atom;
    export struct CondCase;
    export struct Cond;
    export struct Block;
    export struct ArrayLiteral;
    export struct Lambda;
    export struct Lhs;
    export struct Call;
    export struct Unary;
    export struct Binary;
    export struct Assign;

    export using ExprNode = Expr<Atom, Cond, Block, ArrayLiteral, Lambda, Lhs, Call, Unary, Binary, Assign>;
    export using ExprPtr = std::unique_ptr<ExprNode>;

    struct Atom {
        Token token;
    };

    struct CondCase {
        ExprPtr check;
        ExprPtr result;
        bool is_last; //? NOTE: if true, this represents an `else` clause
    };

    struct Cond {
        std::vector<CondCase> cases;
    };

    struct Block {
        std::vector<StmtPtr> stmts;
    };

    struct ArrayLiteral {
        std::vector<ExprPtr> items;
    };

    struct Lambda {
        std::vector<Param> params;
        // std::vector<Token> captures; //! TODO
        ExprPtr body;
    };

    struct Lhs {
        ExprPtr lhs;
        ExprPtr rhs;
        bool uses_dot; //? NOTE: if false, bracket access syntax applies
    };

    struct Call {
        std::vector<ExprPtr> args; 
        ExprPtr callee;
    };

    struct Unary {
        ExprPtr inner;
        AstOp op;
    };

    struct Binary {
        ExprPtr lhs;
        ExprPtr rhs;
        AstOp op;
    };

    struct Assign {
        ExprPtr dest;
        ExprPtr src;
    };


    struct VarDecl {
        Token name;
        ExprPtr initializer;
    };

    struct Vars {
        std::vector<VarDecl> decls;
        bool has_mut;
    };

    struct FunctionDecl {
        std::vector<Param> params;
        Token name_token;
        ExprPtr body;
    };

    struct SymbolDef {
        Token symbol;
        Token prc_op;
        ExprPtr op_lambda;
    };

    struct ExprStmt {
        ExprPtr inner;
    };

    export using AllDecls = std::vector<StmtPtr>;
}
