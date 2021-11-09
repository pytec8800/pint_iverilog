// clang-format off
#include "multi_assign.h"
#include "use_def.h"
#include "assign_cnt.h"
#include "syntax_tree.h"
#include "dom_tree.h"
#include "codegen_helper.h"

using stmt_tree_t = syntaxtree_t<ivl_statement_t>;
using ud_cfg_domtree_t = domtree_t<typename ud_solver_t::graph_t>;
using ud_cfg_postdomtree_t = domtree_t<typename ud_solver_t::graph_t, true>;

void display_massign_result_2(FILE *file, const up_down_t *p_result_map) {
    const auto &result_map = *p_result_map;
    // fprintf(file, "\n");
    for (const auto &massign_pair : result_map) {
        // fprintf(file, "[0x%x, ", massign_pair.first);
        fprintf(file, "[lineno:%d,stmttype:%d] -> ", // stmt-lineno
                massign_pair.first ? ivl_stmt_lineno(massign_pair.first) : -1,
                massign_pair.first ? ivl_statement_type(massign_pair.first) : -1);
        for (const auto &t : massign_pair.second) {
            // fprintf(file, "[0x%x,", t.first);
            fprintf(file, "[%s, %u], ", ivl_signal_basename(t.first), t.second);
        }
        fprintf(file, "\n");
    }
    // fprintf(file, "\n");
}

template <typename DownMap, typename Vertex, typename CfgGraph, typename StmtVertexMap,
          typename SyntaxTree>
void add_final_down_map(DownMap &final_down_map, Vertex s, ivl_signal_t sig, const CfgGraph &cfg,
                        StmtVertexMap &stmt_node_map, const SyntaxTree &tree) {
    // 1. find the stmt of vertex s, named s-stmt
    auto s_stmt = cfg[s].bb;

    // 2. find s-stmt's father stmt, name s-father-stmt
    auto t = stmt_node_map[s_stmt];
    auto t_vp = tree[t];

    if (boost::in_degree(t, tree) == 1) {
        auto father_node = boost::source(*boost::in_edges(t, tree).first, tree);
        auto father_stmt = tree[father_node].bb;

        assert(final_down_map.find(sig) == final_down_map.end()); // should not exist
        final_down_map[sig] = std::make_pair(father_stmt, t_vp);
    } else { // root vertex
        assert(boost::in_degree(t, tree) == 0);
        final_down_map[sig] = std::make_pair(nullptr, t_vp);
    }
}

template <typename UpMap, typename Vertex, typename CfgGraph, typename StmtVertexMap,
          typename SyntaxTree>
void add_final_up_map(UpMap &final_up_map, Vertex s, ivl_signal_t sig, const CfgGraph &cfg,
                      StmtVertexMap &stmt_node_map, const SyntaxTree &tree) {
    // 1. find the stmt of vertex s, named s-stmt
    auto d_stmt = cfg[s].bb;

    // 2. find s-stmt's father stmt, name s-father-stmt
    auto t = stmt_node_map[d_stmt];
    auto t_vp = tree[t];

    if (boost::in_degree(t, tree) == 1) {
        auto father_node = boost::source(*boost::in_edges(t, tree).first, tree);
        auto father_stmt = tree[father_node].bb;

        assert(final_up_map.find(sig) == final_up_map.end()); // should not exist
        final_up_map[sig] = std::make_pair(father_stmt, t_vp);
    } else { // root vertex
        assert(boost::in_degree(t, tree) == 0);
        assert(final_up_map.find(sig) == final_up_map.end()); // should not exist
        final_up_map[sig] = std::make_pair(nullptr, t_vp);
    }
}

bool check_loopidxsig(
    ivl_statement_t m_bb, 
    ivl_statement_t m0_bb, 
    ivl_signal_t sig_k)
{
    // check 'm0' is 'k=expr'
    ivl_lval_t m0_lval = ivl_stmt_lval(m0_bb, 0);
    if(sig_k != ivl_lval_sig(m0_lval))
    {
        return false;
    }

    // check 'm' is 'k=k+c'
    ivl_lval_t m_lval = ivl_stmt_lval(m_bb, 0);
    ivl_expr_t r_expr = ivl_stmt_rval(m_bb);
    
    if(
        (sig_k!=ivl_lval_sig(m_lval)) ||
        (IVL_EX_BINARY!=ivl_expr_type(r_expr)) ||
        !(
            '+' == ivl_expr_opcode(r_expr) || 
            '-' == ivl_expr_opcode(r_expr)
         )
      )
    {
        return false;
    }
    
    ivl_expr_t op1 = ivl_expr_oper1(r_expr);
    ivl_expr_t op2 = ivl_expr_oper2(r_expr);

    ivl_expr_type_t op1_type = ivl_expr_type(op1);
    ivl_expr_type_t op2_type = ivl_expr_type(op2);

    if(
        !((op1_type==IVL_EX_SIGNAL && op2_type==IVL_EX_NUMBER) ||
        (op1_type==IVL_EX_NUMBER && op2_type==IVL_EX_SIGNAL))
      )
    {
        return false;
    }
    
    ivl_expr_t sig_exp;
    //ivl_expr_t num_exp;
    if(op1_type==IVL_EX_SIGNAL)
    {
        sig_exp = op1;
    }
    else
    {
        sig_exp = op2;
    }
    
    if(sig_k != ivl_expr_signal(sig_exp))
    {
        return false;
    }

    return true;
}

int analysis_massign_v2(
    const ivl_process_t net, 
    up_down_t *p_down,
    up_down_t *p_up, 
    assign_cnt_t *p_assigncnt)
{
    int re = 0;
    ivl_process_type_t process_code = ivl_process_type(net);
    ivl_statement_t stmt = ivl_process_stmt(net); // first statement in process

    auto &down_result = *p_down;
    auto &up_result = *p_up;
    auto &assigncnt = *p_assigncnt;

    /*---- non-array signal processing -----------------------------------------------------*/

    /* 0. create syntax tree */
    stmt_tree_t stmttree(stmt);
    const typename stmt_tree_t::graph_t &tree = stmttree.get_tree();
    std::unordered_map<ivl_statement_t, typename stmt_tree_t::VertexDescriptor>
        stmt_node_map; // statement->tree-node map
    {
        typename stmt_tree_t::VertexIter v_begin, v_end;
        std::tie(v_begin, v_end) = boost::vertices(tree);
        for (auto v_it = v_begin; v_it != v_end; v_it++) {
            const auto &node_info = tree[*v_it];
            // if(node_info.bb_id >= SEQ_START) // exclude root node
            stmt_node_map.insert({node_info.bb, *v_it});
        }
    }
    // debug
    __DBegin(== 10);
    {
        std::string str = std::string("stmt_tree_") + std::to_string(ivl_process_lineno(net));
        stmttree.dump_graph(str.c_str(), stmttree_v_printer<typename stmt_tree_t::graph_t>);
    }
    __DEnd;

    /* 1. non-array signal must-def analysis */
    ud_solver_t d_solver;
    std::unordered_map<
        typename ud_solver_t::VertexDescriptor, 
        typename ud_solver_t::node_property_t> maydef_node_fv_map; // snapshort d_solver(may)
    {
        re = d_solver.create_cfg(stmt, (process_code == IVL_PR_INITIAL) ? 0 : 1);
        if (re != 0)
            return re;

        re = d_solver.solve<true>(def_gen_all, null_kill, definition_meet, definition_transfer);
        assert(re == 0);

        // snapshort d_solver(may def)
        d_solver.visit_vertices([&maydef_node_fv_map](
             const typename ud_solver_t::VertexDescriptor &s,
             const typename ud_solver_t::graph_t &g) -> int 
        {
            maydef_node_fv_map[s] = g[s];
            return 0;
        });

        // debug
        __DBegin(== 10);
        {
            const char *s =
                (process_code == IVL_PR_INITIAL) ? "init_may_def_fg" : "always_may_def_fg";
            d_solver.dump_graph(s, ud_v_printer<typename ud_solver_t::graph_t>);
        }
        __DEnd;

        re = d_solver.solve<true>(null_gen, null_kill, definition_meet_anticipate,
                                  definition_transfer);
        assert(re == 0);

        // debug
        __DBegin(== 10);
        {
            const char *s =
                (process_code == IVL_PR_INITIAL) ? "init_must_def_fg" : "always_must_def_fg";
            d_solver.dump_graph(s, ud_v_printer<typename ud_solver_t::graph_t>);
        }
        __DEnd;
    }

    /* 2. non-array signal assign-cnt analysis */
    assign_cnt_solver_t acnt_solver;
    std::unordered_map<typename assign_cnt_solver_t::VertexDescriptor,
                       typename ud_solver_t::VertexDescriptor>
        vv_ad_map;
    std::unordered_map<typename ud_solver_t::VertexDescriptor,
                       typename assign_cnt_solver_t::VertexDescriptor>
        vv_da_map;
    const auto &d_cfg = d_solver.get_graph();
    {
        acnt_solver.set_graph(d_cfg, d_solver.get_entry(), d_solver.get_exit(), vv_ad_map,
                              vv_da_map);

        re = acnt_solver.solve<true>(acnt_gen, null_acnt_kill, acnt_meet, acnt_transfer);
        assert(re == 0);

        // debug
        __DBegin(== 10);
        {
            acnt_solver.dump_graph("assign_cnt_fg",
                                   assigncnt_v_printer<typename assign_cnt_solver_t::graph_t>);
        }
        __DEnd;

        const auto &acnt_exit_out = acnt_solver.get_graph()[acnt_solver.get_exit()].out;

        for (const auto &e : acnt_exit_out) {
            auto sig = e.m_sig;
            const auto &acnts = e.m_acnts;
            unsigned int cnt = (*std::max_element(acnts.begin(), acnts.end()));
            assigncnt[sig] = cnt;
        }
    }

    /* 3. find up-down point */
    std::unordered_map<ivl_signal_t, std::set<ud_solver_t::VertexDescriptor>> all_def_pos;
    std::unordered_map<ivl_signal_t, std::set<ud_solver_t::VertexDescriptor>> all_use_pos;
    ud_cfg_domtree_t dom_tree(d_cfg, d_solver.get_entry());
    ud_cfg_postdomtree_t post_dom_tree(d_cfg, d_solver.get_entry(), d_solver.get_exit());
    {
        __DBegin(== 10);
        {
            dom_tree.dump_graph("dom_tree", d_cfg);
            post_dom_tree.dump_graph("post_dom_tree", d_cfg);
        }
        __DEnd;
    }
    {
        //// find all def/use pos and store them in all_def_pos/all_use_pos

        // collect def pos
        d_solver.visit_vertices([&all_def_pos](const typename ud_solver_t::VertexDescriptor &s,
                                               const typename ud_solver_t::graph_t &g) -> int {
            const auto &stmt = g[s].bb;
            if (stmt) {
                const auto &code = ivl_statement_type(stmt);
                if (code == IVL_ST_ASSIGN /*|| code == IVL_ST_ASSIGN_NB*/) {
                    for (unsigned int idx = 0; idx < ivl_stmt_lvals(stmt); idx++) {
                        ivl_lval_t lval = ivl_stmt_lval(stmt, idx);

                        ivl_lval_t lval_nest = ivl_lval_nest(lval);
                        assert(lval_nest == nullptr && "not support lvalue-nest.");

                        ivl_signal_t sig = ivl_lval_sig(lval);
                        assert(sig);

                        if (ivl_signal_array_count(sig) == 1) // not array
                        {
                            // fprintf(stdout, "def-sig:%s\n", ivl_signal_basename(sig));
                            all_def_pos[sig].emplace(s);
                        }
                        else{ // array
                            all_def_pos[sig].emplace(s);
                            //m_arrays.emplace(sig);
                        }
                    }
                }
            }
            return 0;
        });

        // collect use pos
        d_solver.visit_vertices(
            [&all_def_pos, &all_use_pos](const typename ud_solver_t::VertexDescriptor &s,
                                         const typename ud_solver_t::graph_t &g) -> int {
                const auto &stmt = g[s].bb;
                if (stmt) {
                    auto code = ivl_statement_type(stmt);
                    std::set<ivl_signal_t> use_sigs;

                    int re = collect_all_sigs_exclude_lval(stmt, use_sigs);
                    assert(re == 0);

                    // def-pos that using part select should be regard as a use-pos
                    if (code == IVL_ST_ASSIGN /*|| code == IVL_ST_ASSIGN_NB*/) {
                        for (unsigned int idx = 0; idx < ivl_stmt_lvals(stmt); idx++) {
                            ivl_lval_t lval = ivl_stmt_lval(stmt, idx);
                            ivl_signal_t sig = ivl_lval_sig(lval);
                            assert(sig);

                            ivl_expr_t loff = ivl_lval_part_off(lval);
                            if (loff) {
                                use_sigs.emplace(sig);
                            }
                        }
                    }

                    for (const auto &sig : use_sigs) {
                        auto search = all_def_pos.find(sig);
                        if (search != all_def_pos.end()) // found. sig is a lsig
                        {
                            // warning: vertex may be repeat!! better use std::set instead
                            // std::vector
                            all_use_pos[sig].emplace(s);
                        }
                    }
                }
                return 0;
            });

        // processing def pos in loop
        auto vis_backedges = [&dom_tree,
                              &all_def_pos](const typename ud_solver_t::VertexDescriptor &s,
                                            const typename ud_solver_t::graph_t &g) -> int {
            const auto &bb = g[s].bb;
            if (bb) {
                const auto &code = ivl_statement_type(bb);
                if (code == IVL_ST_WHILE || code == IVL_ST_DO_WHILE ||
                    code == IVL_ST_REPEAT /* || code==IVL_ST_FOREVER*/
                    ) {
                    std::vector<ud_solver_t::EdgeDescriptor> back_edges;
                    std::vector<ud_solver_t::EdgeDescriptor> other_edges;

                    assert(boost::in_degree(s, g) >= 2);

                    typename ud_solver_t::InEdgeIter e_it, e_end;
                    std::tie(e_it, e_end) = boost::in_edges(s, g);
                    for (; e_it != e_end; e_it++) {
                        auto prev_vertex = boost::source(*e_it, g);
                        if (dom_tree.is_dom(s, prev_vertex)) {
                            back_edges.emplace_back(*e_it);
                        } else {
                            other_edges.emplace_back(*e_it);
                        }
                    }

                    if (!back_edges.empty()) { // here has loop.
                        const signal_ud_list_t &out = g[s].out;

                        /* handle non-array */
                        for (const auto &e : back_edges) {
                            const signal_ud_list_t &prev_out = g[boost::source(e, g)].out;

                            // prev_out - out
                            // you d better use may-def
                            signal_ud_list_t non_array_res;
                            setop_extract(non_array_res, prev_out, out);

                            // move loop signal to def_pos.
                            for (const auto &e : non_array_res) {
                                all_def_pos[e.m_sig].emplace(s);
                            }
                        }
                    }
                }
            }
            return 0;
        };
        d_solver.visit_vertices(vis_backedges);

        using parent_child_pair_t =
            std::pair<ivl_statement_t, typename stmt_tree_t::node_property_t>;

        // compute down point of every signal
        std::unordered_map<ivl_signal_t, parent_child_pair_t> final_down_map;
        for (const auto &pair : all_def_pos) {
            const auto &sig = pair.first;
            const auto &def_pos = pair.second;

            //if (def_pos.size() > 1) // include loop entry. just for optimize analysis time
            if (def_pos.size() >1) // include loop entry. just for optimize analysis time
            {
                // auto post_lca = dom_tree.plain_post_lca(def_pos);
                auto post_lca = post_dom_tree.plain_lca(def_pos.begin(), def_pos.end());
                add_final_down_map(final_down_map, post_lca, sig, d_cfg, stmt_node_map, tree);

                // add post-lca to use-pos-set
                // assert( all_use_pos.find(sig) != all_use_pos.end() ); // must exist
                // fprintf(stdout, "down-sig:%s\n", ivl_signal_basename(sig));
                all_use_pos[sig].emplace(post_lca);
            }
            else if(def_pos.size()==1 && assigncnt[sig]>1) // mul-entry-loop
            {
                auto post_lca = post_dom_tree.plain_lca(def_pos.begin(), def_pos.end());
                assert(post_lca == *def_pos.begin());
                add_final_down_map(final_down_map, post_lca, sig, d_cfg, stmt_node_map, tree);
                // Dont add to use_pos_set
            }
        }

        // filter out use-def-set that must-def will flow into.
        for (auto &pair : all_use_pos) {
            auto &sig = pair.first;
            auto &use_pos = pair.second;

            auto rem_fn = [&d_cfg, &sig](const typename ud_solver_t::VertexDescriptor &s) -> bool {
                const auto& in = d_cfg[s].in;
                auto search = in.find(sig);
                return search != in.end(); // true: must-def comes current node
                //const auto &out = d_cfg[s].out; // why use out?? [yzeng]
                //auto search = out.find(sig);
                //return search != out.end();
            };

            for (auto it = use_pos.begin(); it != use_pos.end();) {
                if (rem_fn(*it) == true) {
                    it = use_pos.erase(it);
                } else {
                    ++it;
                }
            }

            // auto delete_first = std::remove_if(use_pos.begin(), use_pos.end(), rem_fn);
            // use_pos.erase(delete_first, use_pos.end());
        }

        // compute up point of every signal.
        std::unordered_map<ivl_signal_t, parent_child_pair_t> final_up_map;
        for (const auto &pair : all_use_pos) {
            const auto &sig = pair.first;
            const auto &use_pos = pair.second;

            if (!use_pos.empty()) {
                auto pre_lca = dom_tree.plain_lca(use_pos.begin(), use_pos.end());
                add_final_up_map(final_up_map, pre_lca, sig, d_cfg, stmt_node_map, tree);
            }
        }

        // result organize
        for (const auto &pair : final_down_map) {
            ivl_signal_t sig;
            ivl_statement_t parent;
            typename stmt_tree_t::node_property_t src_vp;

            sig = pair.first;
            std::tie(parent, src_vp) = pair.second;

            int re = add_multiassign_result<typename stmt_tree_t::node_property_t>(
                sig, nullptr, nullptr, parent, src_vp, down_result);
            assert(re == 0);
        }
        for (const auto &pair : final_up_map) {
            ivl_signal_t sig;
            ivl_statement_t parent;
            typename stmt_tree_t::node_property_t src_vp;

            sig = pair.first;
            std::tie(parent, src_vp) = pair.second;

            int re = add_multiassign_result<typename stmt_tree_t::node_property_t>(
                sig, nullptr, nullptr, parent, src_vp, up_result);
            assert(re == 0);
        }
    }

    /*---- array signal processing ---------------------------------------------------------*/
    
    // processing array in loop
    std::set<std::pair<ivl_statement_t, ivl_lval_t>> forloop_dont_gen;
    std::unordered_multimap<ivl_statement_t, ivl_signal_t> forloopheader_gen;
    collect_lval_array_t larray_finder;
    assign_cnt_solver_t acnt_arr_solver;
    {
        auto vis_backedges = [
            &larray_finder, &maydef_node_fv_map, 
            &forloop_dont_gen, &forloopheader_gen, 
            &dom_tree](
                const typename ud_solver_t::VertexDescriptor &s,
                const typename ud_solver_t::graph_t &g) -> int 
        {
            const auto& bb = g[s].bb;
            if(bb){
                const auto& code = ivl_statement_type(bb);
                if(
                    code==IVL_ST_WHILE || 
                    code==IVL_ST_DO_WHILE || 
                    code==IVL_ST_REPEAT/* || code==IVL_ST_FOREVER*/
                  )
                {
                    std::vector<ud_solver_t::EdgeDescriptor> back_edges;
                    std::vector<ud_solver_t::EdgeDescriptor> other_edges;
                    
                    assert(boost::in_degree(s, g)>=2);
                    
                    if(2 != boost::in_degree(s, g))
                    {
                        return 0;
                    }

                    typename ud_solver_t::InEdgeIter e_it, e_end;
                    std::tie(e_it, e_end) = boost::in_edges(s, g);
                    for(; e_it!=e_end; e_it++){
                        auto prev_vertex = boost::source(*e_it, g);
                        if(dom_tree.is_dom(s, prev_vertex)){
                            back_edges.emplace_back(*e_it);
                        }
                        else{
                            other_edges.emplace_back(*e_it);
                        }
                    }
                    
                    if(!back_edges.empty())
                    {
                        assert(back_edges.size() == 1);
                        assert(boost::in_degree(s, g) == 2);
                        
                        auto m = boost::source(back_edges[0], g);
                        auto m0 = boost::source(other_edges[0], g);
                        
                        ivl_statement_t m_bb = g[m].bb;
                        ivl_statement_t m0_bb = g[m0].bb;
                        ivl_statement_t n_bb = g[s].bb;
                        
                        typename collect_lval_array_t::ValueType arr_def_pos; // pos in loop
                        
                        larray_finder(n_bb, &arr_def_pos);
                        
                        // remove that array-def-cnt>1 in forloop
                        std::map<ivl_signal_t, size_t> arr_loopcnt;
                        for(const auto& e: arr_def_pos)
                        {
                            ivl_lval_t lval = e.second;
                            ivl_signal_t sig = ivl_lval_sig(lval);
                            assert(ivl_signal_array_count(sig)>1);
                            if(arr_loopcnt.find(sig) != arr_loopcnt.end()) // found
                            {
                                arr_loopcnt[sig]++;
                            }
                            else{
                                arr_loopcnt[sig]=1;
                            }
                        }
                        auto rem_first = std::remove_if(arr_def_pos.begin(), arr_def_pos.end(),
                            [&arr_loopcnt](
                                const std::pair<ivl_statement_t, ivl_lval_t>& e)
                                ->bool
                            {
                                ivl_lval_t lval = e.second;
                                ivl_signal_t sig = ivl_lval_sig(lval);
                                assert(arr_loopcnt[sig]>=1);
                                return (arr_loopcnt[sig]>1);
                            });
                        arr_def_pos.erase(rem_first, arr_def_pos.end());

                        for(const auto& e: arr_def_pos)
                        {
                            //ivl_statement_t stmt = e.first;
                            ivl_lval_t lval = e.second;
                            ivl_signal_t sig = ivl_lval_sig(lval);
                            assert(ivl_signal_array_count(sig)>1);
                            
                            ivl_expr_t arr_idx = ivl_lval_idx(lval);
                            
                            if(ivl_expr_type(arr_idx)==IVL_EX_SIGNAL)
                            {
                                ivl_signal_t sig_k = ivl_expr_signal(arr_idx);
                                bool is_loopK = check_loopidxsig(m_bb, m0_bb, sig_k);
                                if(is_loopK) 
                                {
                                    // Here 'sig_k' is induction variable.
                                    forloop_dont_gen.emplace(e);
                                    /*
                                    Note: We only handle the for-loop-header that must
                                        have two incoming edge, one is loop-backedge, the other 
                                        one is 'i=xxx'. so we move the 'A[i]' to the 'i=xxx'
                                        node. 
                                    'm0_bb' is statement of 'i=xxx'.
                                    */
                                    forloopheader_gen.emplace(std::make_pair(m0_bb, sig));
                                }
                            }
                        }
                    }
                }
            }
            return 0;
        };
        d_solver.visit_vertices(vis_backedges);

        // do assign_cnt analysis for array
        vv_ad_map.clear();
        vv_da_map.clear();
        acnt_arr_solver.set_graph(
            d_cfg, 
            d_solver.get_entry(), 
            d_solver.get_exit(),
            vv_ad_map,
            vv_da_map);
        //acnt_arr_solver.clear_dfa();
        
        std::set<ivl_signal_t> arr_and_part;
        acnt_gen_s<decltype(forloop_dont_gen), decltype(forloopheader_gen)>
            acnt_gen_fn(forloop_dont_gen, forloopheader_gen, arr_and_part);
        
        // debug
        __DBegin(==10);
        {
            fprintf(stderr, "\nacnt_arr_solver.solve:\n");
        }
        __DEnd;
        
        // generate at loopheader's output, base on forloopheader_gen
        //acnt_arr_solver.visit_vertices([&forloopheader_gen](
        //    const typename assign_cnt_solver_t::VertexDescriptor &s,
        //    const typename assign_cnt_solver_t::graph_t &g) -> int 
        //{
        //    const auto& net = g[s].bb;
        //    if(net)
        //    {
        //        auto search1 = forloopheader_gen.equal_range(net);
        //        auto& out = 
        //            const_cast<signal_acnt_list_t&>(g[s].out);
        //        assert(out.empty());
        //        for(auto it=search1.first; it!=search1.second; it++)
        //        {
        //            sig_acnt_s e(it->second);
        //            for(auto& i: e.m_acnts) i = 1;
        //            out.emplace(std::move(e));
        //        }
        //    }
        //    return 0;
        //});

        re = acnt_arr_solver.solve<true>(
            acnt_gen_fn, null_acnt_kill, 
            acnt_meet, acnt_transfer);
        assert(re==0);
        
        // debug
        __DBegin(==10);
        {
            acnt_arr_solver.dump_graph(
                "assign_cnt_arr_fg", 
                assigncnt_v_printer<typename assign_cnt_solver_t::graph_t>);
        }
        __DEnd;

        __DBegin(==10);
        {
            DBGPrintSet(arr_and_part);
            fprintf(stderr, "forloop_dont_gen:\n");
            for(const auto& e: forloop_dont_gen)
            {
                ivl_statement_t stmt = e.first;
                ivl_lval_t lval = e.second;
                fprintf(stderr, "[stmt:%u, lval:%s], ", 
                    ivl_stmt_lineno(stmt), 
                    ivl_signal_basename(ivl_lval_sig(lval)));
            }
            fprintf(stderr, "\nforloopheader_gen:\n");
            for(const auto& e: forloopheader_gen)
            {
                ivl_statement_t stmt = e.first;
                ivl_signal_t sig = e.second;
                fprintf(stderr, "[stmt:%u, sig:%s], ", 
                    ivl_stmt_lineno(stmt), 
                    ivl_signal_basename(sig));
            }
        }
        __DEnd;

        const auto& acnt_exit_out = 
            acnt_arr_solver.get_graph()[acnt_arr_solver.get_exit()].out;

        for(const auto& e: acnt_exit_out)
        {
            auto sig = e.m_sig;
            const auto& acnts = e.m_acnts;
            unsigned int cnt = (*std::max_element(acnts.begin(), acnts.end()));
            assigncnt[sig] = cnt;
        }

        for(ivl_signal_t sig: arr_and_part)
        {
            assigncnt[sig] = 3; // array && part. set '3' means not certain.
        }
    }

    return re;
}

// clang-format on
