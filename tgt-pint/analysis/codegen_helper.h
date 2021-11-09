#pragma once
#ifndef __CODEGEN_HELPER_H
#define __CODEGEN_HELPER_H
// clang-format off

// Note: not iterate sub-loop stmt
class collect_lval_array_t : public stmt_visitor_s {
    VISITOR_DECL_BEGIN_V3

  public:
    using ValueType = std::vector<std::pair<ivl_statement_t, ivl_lval_t>>;
    //using ValueType = std::multimap<ivl_signal_t, std::pair<ivl_statement_t, ivl_lval_t>>;

    visit_result_t Visit_ASSIGN(
        ivl_statement_t stmt, 
        ivl_statement_t, 
        void* user_data) override
    {
        return handle_assign(
            stmt, nullptr, 
            *static_cast<ValueType*>(user_data));
    }

    visit_result_t handle_assign(
        ivl_statement_t stmt, 
        ivl_statement_t, 
        ValueType& def_pos)
    {
        ivl_statement_type_t code = ivl_statement_type(stmt);
        
        // dont iterate sub-loop
        if(
            code==IVL_ST_WHILE || 
            code==IVL_ST_DO_WHILE || 
            code==IVL_ST_REPEAT || 
            code==IVL_ST_FOREVER
          )
        {
            return VisitResult_Continue;
        }

        for (unsigned int idx = 0; idx < ivl_stmt_lvals(stmt); idx++)
        {
            ivl_lval_t lval = ivl_stmt_lval(stmt, idx);

            ivl_lval_t lval_nest = ivl_lval_nest(lval);
            assert(lval_nest == nullptr && "not support lvalue-nest.");

            ivl_signal_t sig = ivl_lval_sig(lval);
            assert(sig);

            if (ivl_signal_array_count(sig) == 1) // not array
            {}
            else{
                if (code == IVL_ST_ASSIGN)
                {
                    def_pos.emplace_back(std::make_pair(stmt, lval));
                }
                //else {
                //    assert(code == IVL_ST_ASSIGN_NB);
                //    m_nb_arrays.emplace(sig);
                //}
            }
        }
        return VisitResult_Recurse;
    }
};

template <typename VertexProperty, typename ResultMap>
int add_multiassign_result(ivl_signal_t sig, ivl_statement_t /*src*/, ivl_statement_t /*dst*/,
                           ivl_statement_t src_parent,
                           const VertexProperty &src_vp, // tree vertex property.
                           ResultMap &result_map) {
    ivl_statement_t key;
    unsigned int flag;

    // if father-stmt == nullptr, ignore src_vp.
    if (src_parent == nullptr) {
        // handle exit node
        key = nullptr;
        flag = 0;
    } else {
        // auto src_type = ivl_statement_type(src);
        // auto dst_type = ivl_statement_type(dst);
        auto parent_type = ivl_statement_type(src_parent);
        switch (parent_type) {
        case IVL_ST_BLOCK: {
            key = src_parent;
            flag = src_vp.sub_idx; // if src_type==wait, flag is inserted before sub_idx
            break;
        }
        case IVL_ST_REPEAT:
        case IVL_ST_WHILE: {
            key = src_parent;
            flag = 0; // check after idx=0
            break;
        }
        case IVL_ST_CONDIT: {
            key = src_parent;
            flag = src_vp.is_true_part;
            break;
        }
        case IVL_ST_WAIT:
        case IVL_ST_DELAYX:
        case IVL_ST_DELAY: {
            key = src_parent;
            flag = 0; // sub_idx. insert after sub_idx
            break;
        }
        case IVL_ST_CASE:
        case IVL_ST_CASEX:
        case IVL_ST_CASEZ:
        case IVL_ST_CASER: {
            key = src_parent;
            flag = src_vp.sub_idx;
            break;
        }

        case IVL_ST_FORK:
        case IVL_ST_FORK_JOIN_ANY:
        case IVL_ST_FORK_JOIN_NONE:
        case IVL_ST_FOREVER:
        case IVL_ST_DO_WHILE:
        case IVL_ST_TRIGGER: {
            fprintf(stderr, "\nNot support yet! parent_type:%d. filename:%s, lineno:%d\n",
                    parent_type, __FILE__, __LINE__);
            // NetAssertWrongPath(src_parent);
            return -1;
            // break;
        }
        default: {
            fprintf(stderr, "\nError. parent_type:%d. filename:%s, lineno:%d\n", parent_type,
                    __FILE__, __LINE__);
            // NetAssertWrongPath(src_parent);
            return -1;
        }
        }
    }

    if (result_map.find(key) != result_map.end()) {
        result_map[key].insert({sig, flag});
    } else { // not found
        result_map[key].insert({sig, flag});
    }

    return 0;
}

// clang-format on
#endif
