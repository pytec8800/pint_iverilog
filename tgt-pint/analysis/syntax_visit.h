#pragma once
#ifndef __SYNTAX_VISIT_H__
#define __SYNTAX_VISIT_H__

// clang-format off
#include "common.h"
#include <unordered_set>
#include "common.h"

using CodeTypeForHash = std::ptrdiff_t;

template <typename _Code, typename _Fn> struct func_map_helper {
public:
  func_map_helper(_Code &&code, _Fn &&fn, std::unordered_map<_Code, _Fn> &map) {
    map.emplace(std::make_pair(code, fn));
  }
};

#define VISITOR_DECL_BEGIN(classname, NType)                                   \
  using CodeType = CodeTypeForHash;                                            \
  using NodeType = NType;                                                      \
  using this_type = classname;                                                 \
  using VisitFnType =                                                          \
      std::_Mem_fn<int (this_type::*)(const NodeType &, void *)>;              \
  std::unordered_map<CodeType, VisitFnType> m_fn_map;

#define VISITOR_DECL_BEGIN_V2(classname)                                       \
    using this_type = classname;                                                 \
    using VisitFnType =                                                          \
        std::_Mem_fn<int (this_type::*)(const NodeType &, void *)>;              \
    std::unordered_map<CodeType, VisitFnType> m_fn_map;\
    \
    int operator()(const NodeType& net, void* data){/* functor define */\
      return this->__start_visit__(net, data, *this, m_fn_map);\
    }

#define VISITOR_DECL_BEGIN_V3                                       \
public:\
    visit_result_t operator()(NodeType net, void* data){/* functor define */\
      return this->__start_visit__(net, data);\
    }

/*Define mem_function of current visitor class.
  And construct the mem_func<->code mapping. */
#define VisitFuncDef(code, node, user_data)                                    \
  func_map_helper<CodeType, VisitFnType> Concat2(__fn_map_helper_, code){      \
      static_cast<CodeType>(code),                                             \
      std::mem_fn(&this_type::Concat2(Visit_, code)), m_fn_map};               \
  int Concat2(Visit_, code) (const NodeType &node, void *user_data)

#define HandleCallBack(expr) \
    do{\
        visit_result_t re = (expr);\
        if(re != VisitResult_Recurse){\
            assert(re == VisitResult_Continue || re == VisitResult_Break);\
            return re;\
        }\
    }while(false)

#define HandleTreeTraverse(expr) \
    do{\
        visit_result_t re = (expr);\
        if(re == VisitResult_Break){\
            return re;\
        }\
    }while(false)

// refine ivl_statement visitor.
// ===========================================================================================
typedef enum visit_result_e {
    VisitResult_Recurse = 0,
    VisitResult_Continue  = 1,
    VisitResult_Break  = 2
} visit_result_t;

/*
visit_result_t call_back(ivl_statement_t stmt, 
                         ivl_statement_t parent_stmt, 
                         void* user_data);
*/
template <typename StmtCallBack>
inline visit_result_t 
tree_traverse_stmt(
    ivl_statement_t net,
    ivl_statement_t parent,
    StmtCallBack &&cb, 
    void *user_data = nullptr) 
{
    assert(net != nullptr);
    const ivl_statement_type_t code = ivl_statement_type(net);

    switch (code) {
    case IVL_ST_ALLOC:
    case IVL_ST_ASSIGN:
    case IVL_ST_ASSIGN_NB:
    case IVL_ST_CASSIGN:
    case IVL_ST_CONTRIB:
    case IVL_ST_DEASSIGN:
    case IVL_ST_DISABLE:
    case IVL_ST_FORCE:
    case IVL_ST_FREE:
    case IVL_ST_NOOP:
    case IVL_ST_NONE:
    case IVL_ST_RELEASE:
    case IVL_ST_STASK:
    case IVL_ST_TRIGGER:
    case IVL_ST_UTASK: {
        HandleCallBack( cb(net, parent, user_data) );
        break;
    }
    case IVL_ST_CASEX:
    case IVL_ST_CASEZ:
    case IVL_ST_CASER:
    case IVL_ST_CASE: {
        HandleCallBack( cb(net, parent, user_data) );
        unsigned int cnt = ivl_stmt_case_count(net);
        for (unsigned int i = 0; i < cnt; i += 1) {
            ivl_statement_t st = ivl_stmt_case_stmt(net, i);
            HandleTreeTraverse( tree_traverse_stmt(st, net, cb, user_data) );
        }
        break;
    }
    case IVL_ST_CONDIT: {
        HandleCallBack( cb(net, parent, user_data) );
        ivl_expr_t ex = ivl_stmt_cond_expr(net);
        assert(ex);

        ivl_statement_t t = ivl_stmt_cond_true(net);
        ivl_statement_t f = ivl_stmt_cond_false(net);

        if (t){
            HandleTreeTraverse( tree_traverse_stmt(t, net, cb, user_data) );
        }
        
        if (f){
            HandleTreeTraverse( tree_traverse_stmt(f, net, cb, user_data) );
        }
        break;
    }
    case IVL_ST_DO_WHILE: {
        auto sub = ivl_stmt_sub_stmt(net);
        HandleTreeTraverse( tree_traverse_stmt(sub, net, cb, user_data) );
        HandleCallBack( cb(net, parent, user_data) );
      break;
    }
    case IVL_ST_FORK:
    case IVL_ST_FORK_JOIN_ANY:
    case IVL_ST_FORK_JOIN_NONE: 
    case IVL_ST_BLOCK:{
        HandleCallBack( cb(net, parent, user_data) );
        unsigned int cnt = ivl_stmt_block_count(net);
        for (unsigned int idx = 0; idx < cnt; idx += 1) {
            auto cur = ivl_stmt_block_stmt(net, idx);
            HandleTreeTraverse( tree_traverse_stmt(cur, net, cb, user_data) );
        }
        break;
    }
    case IVL_ST_DELAY:
    case IVL_ST_DELAYX:
    case IVL_ST_WAIT: 
    case IVL_ST_WHILE: 
    case IVL_ST_REPEAT:
    case IVL_ST_FOREVER:{
        HandleCallBack( cb(net, parent, user_data) );
        auto sub = ivl_stmt_sub_stmt(net);
        HandleTreeTraverse( tree_traverse_stmt(sub, net, cb, user_data) );
        break;
    }
    default: {
        NetAssertWrongPath(net);
        //DFA_NOT_SUPPORT(net);
        //assert(0); // error
        break;
    }
    }
    return VisitResult_Recurse;
}

#define StmtCase(name) \
    case Concat2(IVL_ST_, name):{ \
        return Concat2(Visit_, name) (net, parent, user_data); \
    }

class stmt_visitor_s {
public:
    using NodeType = ivl_statement_t;

    visit_result_t __start_visit__(
        NodeType net, 
        void *user_data)
    {
      auto dispatcher_visitor = [this](
          NodeType net, 
          NodeType parent, 
          void *data) -> visit_result_t 
      {
        visit_result_t re = 
            this->Visit_all(net, parent, data);
        return re;
      };
      
      // 'parent==nullptr' indicates net is root node
      return tree_traverse_stmt(net, nullptr, dispatcher_visitor, user_data);
    }

public:
    virtual visit_result_t Visit_all(NodeType net, NodeType parent, void* user_data){
        const ivl_statement_type_t code = ivl_statement_type(net);
        switch (code) {
            StmtCase(NONE)
            StmtCase(NOOP)
            StmtCase(ALLOC)
            StmtCase(ASSIGN)
            StmtCase(ASSIGN_NB)
            StmtCase(BLOCK)
            StmtCase(CASE)
            StmtCase(CASER)
            StmtCase(CASEX)
            StmtCase(CASEZ)
            StmtCase(CASSIGN)
            StmtCase(CONDIT)
            StmtCase(CONTRIB)
            StmtCase(DEASSIGN)
            StmtCase(DELAY)
            StmtCase(DELAYX)
            StmtCase(DISABLE)
            StmtCase(DO_WHILE)
            StmtCase(FORCE)
            StmtCase(FOREVER)
            StmtCase(FORK)
            StmtCase(FORK_JOIN_ANY)
            StmtCase(FORK_JOIN_NONE)
            StmtCase(FREE)
            StmtCase(RELEASE)
            StmtCase(REPEAT)
            StmtCase(STASK)
            StmtCase(TRIGGER)
            StmtCase(UTASK)
            StmtCase(WAIT)
            StmtCase(WHILE)
        }
        assert(nullptr=="ERROR: unreadchable execution path!");
        return VisitResult_Recurse;
    }

    virtual visit_result_t Visit_NONE           (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_NOOP           (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_ALLOC          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_ASSIGN         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_ASSIGN_NB      (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_BLOCK          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_CASE           (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_CASER          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_CASEX          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_CASEZ          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_CASSIGN        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_CONDIT         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_CONTRIB        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_DEASSIGN       (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_DELAY          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_DELAYX         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_DISABLE        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_DO_WHILE       (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_FORCE          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_FOREVER        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_FORK           (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_FORK_JOIN_ANY  (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_FORK_JOIN_NONE (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_FREE           (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_RELEASE        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_REPEAT         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_STASK          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_TRIGGER        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_UTASK          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_WAIT           (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_WHILE          (NodeType, NodeType, void*) {return VisitResult_Recurse;}

public:
    visit_result_t visitor_status_;
};


//// refine ivl_expr_t visitor
//============================================================================================
#define ExprCase(name) \
    case Concat2(IVL_EX_, name):{ \
        return Concat2(Visit_, name) (net, parent, user_data); \
    }

// get succ-array of specified expression
inline
void dfa_expr_succs(const ivl_expr_t& node, std::vector<ivl_expr_t>& succ_vec)
{
    ivl_expr_type_t code = ivl_expr_type(node);
    switch (code) {
        // 1 oper
        //case IVL_EX_ARRAY: // shared with IVL_EX_SIGNAL
        case IVL_EX_SIGNAL:
        case IVL_EX_MEMORY:
        case IVL_EX_UNARY:
        case IVL_EX_PROPERTY:{
            //succ_vec.resize(1);
            //succ_vec[0] = ivl_expr_oper1(node);
            succ_vec.clear();
            ivl_expr_t e;
            e = ivl_expr_oper1(node);
            if(e) succ_vec.emplace_back(e);
            break;
        }

        // 2 oper
        case IVL_EX_BINARY:
        case IVL_EX_SELECT:
        case IVL_EX_SHALLOWCOPY:
        case IVL_EX_NEW:{
            succ_vec.clear();
            ivl_expr_t e;
            
            e = ivl_expr_oper1(node);
            if(e) succ_vec.emplace_back(e);
            
            e = ivl_expr_oper2(node);
            if(e) succ_vec.emplace_back(e);

            //succ_vec.resize(2);
            //succ_vec[0] = ivl_expr_oper1(node);
            //succ_vec[1] = ivl_expr_oper2(node);
            break;
        }

        // 3 oper
        case IVL_EX_TERNARY:{
            //succ_vec.resize(3);
            //succ_vec[0] = ivl_expr_oper1(node);
            //succ_vec[1] = ivl_expr_oper2(node);
            //succ_vec[2] = ivl_expr_oper3(node);
            succ_vec.clear();
            ivl_expr_t e;
            
            e = ivl_expr_oper1(node);
            if(e) succ_vec.emplace_back(e);
            
            e = ivl_expr_oper2(node);
            if(e) succ_vec.emplace_back(e);
            
            e = ivl_expr_oper3(node);
            if(e) succ_vec.emplace_back(e);
          break;
        }

        // other. (may be any size)
        case IVL_EX_ARRAY_PATTERN:
        case IVL_EX_CONCAT:
        case IVL_EX_SFUNC:
        case IVL_EX_UFUNC:{
            //size = ivl_expr_parms(node);
            //succ_vec.resize(size);
            //for(unsigned int i=0; i<size; i++){
            //  succ_vec[i] = ivl_expr_parm(node, i);
            //}
            succ_vec.clear();
            ivl_expr_t e;
            unsigned int size = ivl_expr_parms(node);
            for(unsigned int i=0; i<size; i++){
                e = ivl_expr_parm(node, i);
                if(e) succ_vec.emplace_back(e);
            }
            break;
        }

        // 0 oper
        case IVL_EX_NONE:
        case IVL_EX_BACCESS: // 0 oper
        case IVL_EX_DELAY: // 0 oper
        case IVL_EX_ENUMTYPE: // 0 oper
        case IVL_EX_EVENT: // 0 oper
        case IVL_EX_NULL: // 0 oper
        case IVL_EX_NUMBER: // 0 oper
        case IVL_EX_REALNUM: // 0 oper
        case IVL_EX_SCOPE: // 0 oper
        case IVL_EX_STRING: // 0 oper
        case IVL_EX_ARRAY: // 0 oper.  IVL_EX_SFUNC / UFUNC
        case IVL_EX_ULONG:{
          succ_vec.clear();
          break;
        }

        default:{
          succ_vec.clear();
          assert(nullptr=="ERROR: unreadchable ececution path!");
        }
    }
    return;
}

/* 
visit_result_t call_back(ivl_expr_t net, 
                         ivl_expr_t parent, 
                         void* user_data);
*/
template<typename ExprCallBack>
inline visit_result_t 
tree_traverse_expr(
    ivl_expr_t net, 
    ivl_expr_t parent,
    ExprCallBack&& cb, 
    void* user_data = nullptr)
{
    assert(net);
    HandleCallBack( cb(net, parent, user_data) );
    
    std::vector<ivl_expr_t> succ_vec;
    dfa_expr_succs(net, succ_vec);

    for(const auto& sub_ex: succ_vec){
        HandleTreeTraverse(tree_traverse_expr(sub_ex, net, cb, user_data));
    }
    return VisitResult_Recurse;
}

class expr_visitor_s {
public:
    using NodeType = ivl_expr_t;

    visit_result_t __start_visit__(
        NodeType net, 
        void *user_data)
    {
        auto dispatcher_visitor = [this](
            NodeType net, 
            NodeType parent, 
            void *data) -> visit_result_t 
        {
          visit_result_t re = 
              this->Visit_all(net, parent, data);
          return re;
        };
        
        // 'parent==nullptr' indicates net is root node
        return tree_traverse_expr(net, nullptr, dispatcher_visitor, user_data);
    }

public:
    virtual visit_result_t Visit_all(NodeType net, NodeType parent, void* user_data){
        ivl_expr_type_t code = ivl_expr_type(net);
        switch (code) {
            ExprCase(NONE)
            ExprCase(ARRAY)
            ExprCase(BACCESS)
            ExprCase(BINARY)
            ExprCase(CONCAT)
            ExprCase(DELAY)
            ExprCase(ENUMTYPE)
            ExprCase(EVENT)
            ExprCase(MEMORY)
            ExprCase(NEW)
            ExprCase(NUMBER)
            ExprCase(ARRAY_PATTERN)
            ExprCase(PROPERTY)
            ExprCase(REALNUM)
            ExprCase(SCOPE)
            ExprCase(SELECT)
            ExprCase(SFUNC)
            ExprCase(SHALLOWCOPY)
            ExprCase(SIGNAL)
            ExprCase(STRING)
            ExprCase(TERNARY)
            ExprCase(UFUNC)
            ExprCase(ULONG)
            ExprCase(UNARY)
            /*ExprCase(NULL)*/
            case IVL_EX_NULL:{
                return Visit_NULL(net, parent, user_data);
            }
        }
        NetAssertWrongPath(net);
        //assert(nullptr=="ERROR: unreadchable ececution path!");
        return VisitResult_Recurse;
    }
    virtual visit_result_t Visit_NONE          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_ARRAY         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_BACCESS       (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_BINARY        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_CONCAT        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_DELAY         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_ENUMTYPE      (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_EVENT         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_MEMORY        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_NEW           (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_NULL          (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_NUMBER        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_ARRAY_PATTERN (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_PROPERTY      (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_REALNUM       (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_SCOPE         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_SELECT        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_SFUNC         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_SHALLOWCOPY   (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_SIGNAL        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_STRING        (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_TERNARY       (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_UFUNC         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_ULONG         (NodeType, NodeType, void*) {return VisitResult_Recurse;}
    virtual visit_result_t Visit_UNARY         (NodeType, NodeType, void*) {return VisitResult_Recurse;}

public:
    visit_result_t visitor_status_;
};

// clang-format on
#endif
