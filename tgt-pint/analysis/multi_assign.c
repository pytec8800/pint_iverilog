// clang-format off

#include "multi_assign.h"
#include "use_def.h"
//#include "assign_cnt.h"
#include "syntax_tree.h"
#include "dom_tree.h"
#include "codegen_helper.h"

using stmt_tree_t = syntaxtree_t<ivl_statement_t>;
using ud_cfg_domtree_t = domtree_t<typename ud_solver_t::graph_t>;



void display_massign_result(FILE *file, const dfa_massign_t *p_result_map) {
    const auto &result_map = *p_result_map;
    // fprintf(file, "\n");
    for (const auto &massign_pair : result_map) {
        // fprintf(file, "[0x%x, ", massign_pair.first);
        fprintf(file, "[lineno:%d] -> ", // stmt-lineno
                massign_pair.first ? ivl_stmt_lineno(massign_pair.first) : -1);
        for (const auto &t : massign_pair.second) {
            // fprintf(file, "[0x%x,", t.first);
            fprintf(file, "[%s, %u], ", ivl_signal_basename(t.first), t.second);
        }
        fprintf(file, "\n");
    }
    // fprintf(file, "\n");
}

// template<typename stmt_tree_t, typename ud_solver_t, typename ResMap>
void gen_down_result(
    ivl_statement_t stmt, ud_solver_t &solver, 
    dfa_massign_t &massign_result, 
    dfa_massign_t &up_result) 
{
    // auto entry = solver.get_entry();
    auto exit = solver.get_exit();
    int re = 0;

    std::set<ivl_signal_t> signals;

    // 1. collect all signal(lvalue, to be check)
    auto vis_collect_signal = [&signals](const typename ud_solver_t::VertexDescriptor &s,
                                         const typename ud_solver_t::graph_t &g) -> int {
        const auto &gen = g[s].gen;
        for (auto it = gen.begin(); it != gen.end(); it++) {
            signals.emplace(it->m_sig);
        }
        return 0;
    };
    solver.visit_vertices(vis_collect_signal);

    // 2. create syntax tree
    stmt_tree_t stmttree(stmt);
    const typename stmt_tree_t::graph_t &tree = stmttree.get_tree();
    typename stmt_tree_t::VertexIter v_begin, v_end;
    std::tie(v_begin, v_end) = boost::vertices(tree);
    std::unordered_map<ivl_statement_t, typename stmt_tree_t::VertexDescriptor>
        stmt_node_map; // statement->tree-node map
    for (auto v_it = v_begin; v_it != v_end; v_it++) {
        const auto &node_info = tree[*v_it];
        // if(node_info.bb_id >= SEQ_START) // exclude root node
        stmt_node_map.insert({node_info.bb, *v_it});
    }

    // 3. handle exit node of cfg
    const auto &cfg = solver.get_graph();
    auto exit_out = cfg[exit].out;
    for (const auto &defs : exit_out) {
        auto sig = defs.m_sig;
        assert(!defs.m_stmts.empty());
        // ivl_statement_type_t code = ivl_statement_type(stmt);

        // if father(key) == nullptr, indicate check point should add at exit node
        add_multiassign_result<typename stmt_tree_t::node_property_t>(
            sig, stmt, nullptr, nullptr, tree[stmt_node_map[stmt]], massign_result);

        // remove exit-out signal
        auto search = signals.find(sig);
        assert(search != signals.end());
        signals.erase(search);
    }

    // 4. find out all signal-definition that in_flow_value > 0 && out_flow_value==0
    using parent_child_pair_t = 
        std::pair<ivl_statement_t, typename stmt_tree_t::node_property_t>;
    std::unordered_map<ivl_signal_t, std::vector<parent_child_pair_t>> 
        final_down_map;
    std::unordered_map<ivl_signal_t, std::set<typename ud_solver_t::EdgeDescriptor>> 
        sigs_exit_map;

    ud_cfg_domtree_t dom_tree(cfg, solver.get_entry());

    __DebugBegin(2);
    {
        std::string s = std::string("dom_tree_") 
            + std::to_string(ivl_stmt_lineno(stmt));
        dom_tree.dump_graph(s.c_str(), cfg);
    }
    __DebugEnd;   
    
    //auto dom_entry = dom_tree.get_entry();
    // compute every node's depth in dom-tree

    auto vis_find_exit = 
        [&stmt,
        &sigs_exit_map, &signals, &cfg, 
        &final_down_map, &dom_tree, &tree,
        &stmt_node_map, &up_result/*, &dom_cfg_vertex_map,
        &cfg_dom_vertex_map*/](
            const typename ud_solver_t::VertexDescriptor &s,
            const typename ud_solver_t::graph_t &g) -> int 
    {
        // const signal_ud_list_t& in=g[s].in;
        const signal_ud_list_t &out = g[s].out;

        typename ud_solver_t::InEdgeIter e_it, e_begin, e_end;
        std::tie(e_begin, e_end) = boost::in_edges(s, g);
        
        std::vector<typename ud_solver_t::EdgeDescriptor> exit_edges;
        std::vector<typename ud_solver_t::VertexDescriptor> exit_srcvertex;

        for (const auto &sig : signals) {
            exit_edges.clear();
            exit_srcvertex.clear();
            auto search_out = out.find(sig);
            if (search_out == out.end()) { // not found
                // check all prev node out-flow-value
                unsigned int valid_edges = 0; // valid in-edge for current signal.
                for (e_it = e_begin; e_it != e_end; e_it++) {
                    auto prev_vertex = boost::source(*e_it, g);
                    auto prev_out = g[prev_vertex].out;
                    auto search_in = prev_out.find(sig);
                    if (search_in !=
                        prev_out.end()) { // found. indicate current vertex is exit-node
                        //sigs_exit_map[sig].emplace(*e_it);
                        exit_edges.emplace_back(*e_it);
                        exit_srcvertex.emplace_back(prev_vertex);
                        valid_edges++;
                    }
                }
                if (0 < valid_edges && valid_edges <= MultiDownLimit) {
                    // at least one prev that no 'sig' definition
                    assert((boost::in_degree(s, g) > valid_edges));
                    for(const auto& e: exit_edges){
                        // 1. find the stmt of vertex s, named s-stmt
                        auto s_stmt = cfg[boost::source(e, cfg)].bb;
                        //auto d_stmt = cfg[boost::target(e, cfg)].bb;

                        // 2. find s-stmt's father stmt, name s-father-stmt
                        auto t = stmt_node_map[s_stmt];
                        auto t_vp = tree[t];
                        assert(boost::in_degree(t, tree) == 1);
                        auto father_node = boost::source(*boost::in_edges(t, tree).first, tree);
                        auto father_stmt = tree[father_node].bb;
                        
                        final_down_map[sig].emplace_back(std::make_pair(father_stmt, t_vp));
                    }
                }
                else if(valid_edges > MultiDownLimit){ 
                    // merge multi-down in a single down point
                    
                    __DBegin(==5);
                    fprintf(stderr, "Meet MultiDownLimit lineno:%d, signal:%s\n", 
                        ivl_stmt_lineno(stmt), ivl_signal_basename(sig));
                    __DEnd;

                    assert((boost::in_degree(s, g) > valid_edges));
                    
                    // find lowest common ancestor within dominator-tree
                    auto lca_vertex = dom_tree.plain_lca(exit_srcvertex);
                    
                    // add down point
                    auto s_stmt = cfg[lca_vertex].bb;
                    auto t = stmt_node_map[s_stmt];
                    auto t_vp = tree[t];
                    assert(boost::in_degree(t, tree) <= 1);
                    if(boost::in_degree(t, tree) == 1)
                    {
                        auto father_node = boost::source(*boost::in_edges(t, tree).first, tree);
                        auto father_stmt = tree[father_node].bb;
                        final_down_map[sig].emplace_back(std::make_pair(father_stmt, t_vp));
                        // add up point
                        const signal_ud_list_t& d_in = cfg[lca_vertex].in;
                        if(d_in.find(sig) == d_in.end()){ // not found
                            // if has must-def, then no need to 'up' signal
                            int re = add_multiassign_result
                                <typename stmt_tree_t::node_property_t>(
                                    sig, 
                                    nullptr,
                                    nullptr, 
                                    father_stmt,
                                    t_vp,
                                    up_result);
                            assert(re == 0);
                        }
                    } 
                    else{ // dealing with the lac-ancestor that is a entry node
                        // add down at exit
                        final_down_map[sig].emplace_back(std::make_pair(nullptr, t_vp));
                        // add up point at entry
                        int re = add_multiassign_result
                            <typename stmt_tree_t::node_property_t>(
                                sig, 
                                nullptr,
                                nullptr, 
                                nullptr,
                                t_vp, 
                                up_result);
                        assert(re == 0);
                    }
                }
                else{
                }
            }
        }
        return 0;
    };
    // fprintf(stderr, "solver.vertex_num:%u, signal_num:%u\n",
    //    (unsigned int)solver.get_vertices_num(),
    //    (unsigned int)signals.size());
    solver.visit_vertices(vis_find_exit);
    // DBGPrintSet(signals);
    // for(const auto& kv: sigs_exit_map){
    //    fprintf(stderr, "%s:%u, \n",
    //        ivl_signal_basename(kv.first),
    //        (unsigned int)kv.second.size());
    //}
    // fprintf(stderr, "\n");

    // 5.  produce code-gen data-struct
    for(const auto& sig_exit: final_down_map){
        auto sig = sig_exit.first;
        const auto& pair_vec = sig_exit.second;
        
        ivl_statement_t parent;
        typename stmt_tree_t::node_property_t src_vp;
        
        for(const auto& pair: pair_vec){
            std::tie(parent, src_vp) = pair;
            re = add_multiassign_result<typename stmt_tree_t::node_property_t>(
                sig, 
                nullptr/*s_stmt*/,
                nullptr/*d_stmt*/, 
                parent,
                src_vp, // tree vertex property
                massign_result);
            assert(re == 0);
        }
    }

    // debug
    // display_massign_result(stderr, &massign_result);
}

void gen_up_result(ivl_statement_t stmt, ud_solver_t &u_solver, ud_solver_t &d_solver,
                   std::set<ivl_signal_t> d_signals, // definition signals
                   dfa_massign_t &up_result) {
    auto entry = u_solver.get_entry();
    int re = 0;

    // std::set<ivl_signal_t> signals;
    //// 1. collect all signal's defs.
    //// NOTE: There is no need to collect use-only signals, because we do not
    ////       care about signals that has no definition within a ivl_process_t
    ////       during multi-assign analysis so far.
    // auto vis_collect_signal = [&signals](const typename ud_solver_t::VertexDescriptor &s,
    //                                     const typename ud_solver_t::graph_t &g) -> int {
    //    const auto &gen = g[s].gen;
    //    for (auto it = gen.begin(); it != gen.end(); it++) {
    //        signals.emplace(it->m_sig);
    //    }
    //    return 0;
    //};
    // TimingBegin("gen_up_result: 1");
    //// we dont care about use-only signals, so use d_solver.gen
    // d_solver.visit_vertices(vis_collect_signal);
    // TimingEnd;

    std::set<ivl_signal_t> signals;

    // 1. collect all signal's uses.
    auto vis_collect_signal = [&signals](const typename ud_solver_t::VertexDescriptor &s,
                                         const typename ud_solver_t::graph_t &g) -> int {
        const auto &gen = g[s].gen;
        for (auto it = gen.begin(); it != gen.end(); it++) {
            signals.emplace(it->m_sig);
        }
        return 0;
    };
    TimingBegin("gen_up_result: 1");
    u_solver.visit_vertices(vis_collect_signal);
    {
        std::set<ivl_signal_t> tmp;
        for (const auto &sig : d_signals) { // intersect
            auto search = signals.find(sig);
            if (search != signals.end()) { // found
                tmp.emplace(sig);
            }
        }
        signals = std::move(tmp);
    }
    TimingEnd;

    // 2. create syntax tree
    // TimingBegin("gen_up_result: 2");
    stmt_tree_t stmttree(stmt);
    const typename stmt_tree_t::graph_t &tree = stmttree.get_tree();
    typename stmt_tree_t::VertexIter v_begin, v_end;
    std::tie(v_begin, v_end) = boost::vertices(tree);
    std::unordered_map<ivl_statement_t, typename stmt_tree_t::VertexDescriptor>
        stmt_node_map; // statement->tree-node map
    for (auto v_it = v_begin; v_it != v_end; v_it++) {
        const auto &node_info = tree[*v_it];
        // if(node_info.bb_id >= SEQ_START) // exclude root node
        stmt_node_map.insert({node_info.bb, *v_it});
    }

    __DBegin(== 3);
    for (auto pair : stmt_node_map) {
        fprintf(stderr, "stmt=%p,vertex=%u\n", pair.first, (unsigned int)pair.second);
    }
    __DEnd;
    // TimingEnd;

    // 3. handle entry node of cfg
    const auto &cfg = u_solver.get_graph();
    auto entry_in = cfg[entry].in;
    TimingBegin("gen_up_result: 3");
    for (const auto &uses : entry_in) {
        auto sig = uses.m_sig;
        assert(!uses.m_stmts.empty());
        // ivl_statement_type_t code = ivl_statement_type(stmt);

        // if father(key) == nullptr, indicate check point should add at exit node
        add_multiassign_result<typename stmt_tree_t::node_property_t>(
            sig, nullptr, nullptr, nullptr, tree[stmt_node_map[stmt]], /*no use*/
            up_result);

        // remove entry_in signal
        auto search = signals.find(sig); // 'sig' may be not exist in signals.
        // assert(search != signals.end());
        if (search != signals.end()) { // found
            signals.erase(search);
        }
    }
    TimingEnd;

    // 3.5. construct stmt-vertexdesc map
    std::unordered_map<unsigned int, typename ud_solver_t::VertexDescriptor>
        d_bbid_vertex_map; // d_solver's stmt-vertex mapping
    auto vis_stmt_vertex = [&d_bbid_vertex_map](const typename ud_solver_t::VertexDescriptor &s,
                                                const typename ud_solver_t::graph_t &g) -> int {
        auto id = g[s].bb_id;
        d_bbid_vertex_map[id] = s;
        return 0;
    };
    TimingBegin("gen_up_result: 3.5");
    d_solver.visit_vertices(vis_stmt_vertex);
    TimingEnd;

    const auto &d_cfg = d_solver.get_graph();

    // 4. find out all signal-use that out_flow_value > 0 && in_flow_value ==0
    std::unordered_map<ivl_signal_t, std::set<typename ud_solver_t::EdgeDescriptor>> sigs_entry_map;
    auto vis_find_entry = [&sigs_entry_map, &signals, &d_bbid_vertex_map,
                           &d_cfg](const typename ud_solver_t::VertexDescriptor &s,
                                   const typename ud_solver_t::graph_t &g) -> int {
        // TimingBegin("gen_up_result: 4.1");
        const signal_ud_list_t &in = g[s].in;

        typename ud_solver_t::OutEdgeIter e_it, e_begin, e_end;
        std::tie(e_begin, e_end) = boost::out_edges(s, g);

        for (const auto &sig : signals) {
            auto search_out = in.find(sig);
            if (search_out == in.end()) { // not found
                // check all succeed node in-flow-value
                unsigned int valid_edges = 0; // valid in-edge for current signal.
                for (e_it = e_begin; e_it != e_end; e_it++) {
                    auto succ_in = g[boost::target(*e_it, g)].in;
                    auto search = succ_in.find(sig);
                    if (search != succ_in.end()) {
                        // found. indicate current vertex is exit-node
                        auto d_vertex = d_bbid_vertex_map[g[s].bb_id];
                        const signal_ud_list_t &d_out = d_cfg[d_vertex].out;
                        assert(g[s].bb == d_cfg[d_vertex].bb);
                        if (d_out.find(sig) == d_out.end()) {
                            // if has must-def, then no need to 'up' signal
                            sigs_entry_map[sig].emplace(*e_it);
                            valid_edges++;
                        }
                    }
                }
                // if (valid_edges > 0) {
                //    // at least one prev that no 'sig' definition
                //    assert((boost::out_degree(s, g) > valid_edges)); //[no check]
                //}
            }
        }
        return 0;
    };
    TimingBegin("gen_up_result: 4");
    //__DBegin(4);
    // fprintf(stderr, "u_solver.vertex_num:%u, signal_num:%u\n",
    //    (unsigned int)u_solver.get_vertices_num(),
    //    (unsigned int)signals.size());
    if (!signals.empty()) {
        u_solver.visit_vertices(vis_find_entry);
    }
    // DBGPrintSet(signals);
    // for(const auto& kv: sigs_entry_map){
    //    fprintf(stderr, "%s:%u, \n",
    //        ivl_signal_basename(kv.first),
    //        (unsigned int)kv.second.size());
    //}
    // fprintf(stderr, "\n");
    //__DEnd;
    TimingEnd;

    // 5.  produce code-gen data-struct
    TimingBegin("gen_up_result: 5");
    for (const auto &sig_entry : sigs_entry_map) {
        auto sig = sig_entry.first;
        auto exit_edges = sig_entry.second;
        for (const typename ud_solver_t::EdgeDescriptor &e : exit_edges) {
            // 1. find the stmt of vertex s, named s-stmt
            auto s_stmt = cfg[boost::source(e, cfg)].bb;
            auto d_stmt = cfg[boost::target(e, cfg)].bb;

            // 2. find s-stmt's father stmt, name s-father-stmt
            // auto t = stmt_node_map[s_stmt];
            auto t = stmt_node_map[d_stmt];
            auto t_vp = tree[t];
            auto in_de = boost::in_degree(t, tree);
            assert(in_de == 1);
            auto father_node = boost::source(*boost::in_edges(t, tree).first, tree);
            auto father_stmt = tree[father_node].bb;

            // 3. organize result
            re = add_multiassign_result<typename stmt_tree_t::node_property_t>(
                sig, s_stmt, d_stmt, father_stmt,
                t_vp, // tree vertex property
                up_result);
            assert(re == 0);
        }
    }
    TimingEnd;

    // debug
    // display_massign_result(stderr, &up_result);
}







class collect_lval_signal_t : public stmt_visitor_s {
    VISITOR_DECL_BEGIN_V3

  public:
    std::set<ivl_signal_t> &nb_signals;
    std::set<ivl_signal_t> &signals;

    // std::set<ivl_signal_t> tmp_nb_part;
    std::set<ivl_signal_t> tmp_part; // contain nb and non_nb

    collect_lval_signal_t(std::set<ivl_signal_t> &nb_sigs, std::set<ivl_signal_t> &sigs /*,
                                                           std::set<ivl_signal_t> &nb_par,
                                                           std::set<ivl_signal_t> &par*/
                          )
        : nb_signals(nb_sigs), signals(sigs) /*, nb_part(nb_par), part(par) */ {}

  public:
    visit_result_t Visit_ASSIGN(ivl_statement_t stmt, ivl_statement_t, void *) override {
        return handle_assign(stmt, nullptr, nullptr);
    }

    visit_result_t Visit_ASSIGN_NB(ivl_statement_t stmt, ivl_statement_t, void *) override {
        return handle_assign(stmt, nullptr, nullptr);
    }

    visit_result_t handle_assign(ivl_statement_t stmt, ivl_statement_t, void *) {
        ivl_statement_type_t code = ivl_statement_type(stmt);
        for (unsigned int idx = 0; idx < ivl_stmt_lvals(stmt); idx++) {
            ivl_lval_t lval = ivl_stmt_lval(stmt, idx);

            ivl_lval_t lval_nest = ivl_lval_nest(lval);
            assert(lval_nest == nullptr && "not support lvalue-nest.");

            ivl_signal_t sig = ivl_lval_sig(lval);
            assert(sig);

            if (ivl_signal_array_count(sig) == 1) // not array
            {
                if (ivl_lval_part_off(lval) == NULL) { // not part-select
                    auto search = tmp_part.find(sig);
                    if (search == tmp_part.end()) { // not found
                        // part-assign has not appeared before
                        if (code == IVL_ST_ASSIGN) {
                            signals.emplace(sig);
                        } else {
                            assert(code == IVL_ST_ASSIGN_NB);
                            nb_signals.emplace(sig);
                        }
                    }
                } else {
                    tmp_part.emplace(sig);
                    auto search = signals.find(sig);
                    if (search != signals.end()) { // found
                        signals.erase(search);
                    }
                    auto search_nb = nb_signals.find(sig);
                    if (search_nb != nb_signals.end()) { // found
                        nb_signals.erase(search_nb);
                    }
                }
            }
        }
        return VisitResult_Recurse;
        ;
    }

    // false: no same signal between 'nb_signals' and 'signals'
    // true: others
    bool has_same_singal() {
        for (const auto &sig : signals) {
            if (nb_signals.find(sig) != nb_signals.end()) { // found
                return true;
            }
        }
        return false;
    }
};

class test_t : public stmt_visitor_s {
    VISITOR_DECL_BEGIN_V3
  public:
    visit_result_t Visit_BLOCK(ivl_statement_t, ivl_statement_t, void *) override {
        fprintf(stderr, "\ntest_t: %s called\n", __func__);
        return VisitResult_Recurse;
    }
};
class test_t2 : public test_t {
    VISITOR_DECL_BEGIN_V3
  public:
    visit_result_t Visit_ASSIGN_NB(ivl_statement_t, ivl_statement_t, void *) override {
        fprintf(stderr, "\ntest_t2: %s called\n", __func__);
        return VisitResult_Recurse;
    }
};





// Do dataflow analysis for single process.   DFA = "DataFlow Analysis"
int analysis_massign(const ivl_process_t net, dfa_massign_t *p_down_result,
                     dfa_massign_t *p_up_result, std::set<ivl_signal_t> *p_nb_lval_sigs,
                     std::set<ivl_signal_t> *p_lval_sigs) {
    // fprintf(stderr, "\n\n\n\n\n\n\n");
    // test_t2 tt;
    // tt(ivl_process_stmt(net), nullptr);
    // fprintf(stderr, "\n\n\n\n\n\n\n");
    // return -1;

    __DBegin(== 2);
    fprintf(stderr, "\n%s:%d [Debug] process:\n", ivl_process_file(net), ivl_process_lineno(net));
    __DEnd;

    auto &massign_result = *p_down_result;
    auto &up_result = *p_up_result;

    massign_result.clear();
    up_result.clear();

    // collect lvalue signals
    std::set<ivl_signal_t> signals;
    std::set<ivl_signal_t> nb_signals;
    // std::set<ivl_signal_t> part;
    // std::set<ivl_signal_t> nb_part;
    collect_lval_signal_t l_sigs_finder(nb_signals, signals);

    TimingBegin("collect lvalue signals");
    l_sigs_finder(ivl_process_stmt(net), nullptr);
    set_subtract(*p_nb_lval_sigs, nb_signals, signals);
    set_subtract(*p_lval_sigs, signals, nb_signals);
    TimingEnd;

    std::set<ivl_signal_t> all_handle_l_signals;
    all_handle_l_signals.insert(nb_signals.begin(), nb_signals.end());
    all_handle_l_signals.insert(signals.begin(), signals.end());

    // if (l_sigs_finder.has_same_singal())
    //    return -1;

    __DebugBegin(2);
    //DBGPrintSet(nb_signals);
    //DBGPrintSet(signals);
    DBGPrintSet(*p_nb_lval_sigs);
    DBGPrintSet(*p_lval_sigs);
    fprintf(stderr, "\n");
    __DebugEnd;

    switch (ivl_process_type(net)) {
    case IVL_PR_ALWAYS: {
        ivl_statement_t stmt = ivl_process_stmt(net); // first statement in process

        ud_solver_t d_solver;
        ud_solver_t u_solver;

        {
            int re;

            TimingBegin("d_solver.create_cfg");
            re = d_solver.create_cfg(stmt, 1);
            TimingEnd;
            if (re != 0) return re;

            // dataflow solve
            TimingBegin("d_solver.solve may");
            re = d_solver.solve<true>(
                definition_gen, null_kill, definition_meet, definition_transfer);
            TimingEnd;
            assert(re == 0);

            // debug
            __DebugBegin(2);
            {
                std::string s = std::string("always_may_def_fg_") 
                    + std::to_string(ivl_process_lineno(net));
                d_solver.dump_graph(s.c_str());
            }
            __DebugEnd;

            TimingBegin("d_solver.solve must");
            re = d_solver.solve<true>(null_gen, null_kill, definition_meet_anticipate,
                                     definition_transfer /*use set merge*/);
            TimingEnd;
            assert(re == 0);

            // debug
            __DebugBegin(2);
            {
                std::string s = std::string("always_must_def_fg_") 
                    + std::to_string(ivl_process_lineno(net));
                d_solver.dump_graph(s.c_str());
            }
            __DebugEnd;

            // analysis result
            TimingBegin("gen_down_result");
            gen_down_result(stmt, d_solver, massign_result, up_result);
            TimingEnd;
        }

        /* up analysis */
        {
            // int re = u_solver.create_cfg(stmt, 1);
            // assert(re == 0);
            TimingBegin("u_solver set cfg and clear nodes");
            u_solver.set_graph(d_solver.get_graph(), d_solver.get_entry(), d_solver.get_exit());
            u_solver.clear_dfa();
            TimingEnd;

            __DebugBegin(3);
            auto vis_print_bbid = [](typename ud_solver_t::VertexDescriptor s,
                                     typename ud_solver_t::graph_t &g) -> int {
                fprintf(stderr, "[bbid=%u, stmt-no=%d]\n", g[s].bb_id,
                        g[s].bb ? (int)ivl_stmt_lineno(g[s].bb) : -1);
                return 0;
            };
            fprintf(stderr, "\nu_solver bbid: \n");
            u_solver.visit_vertices(vis_print_bbid);
            fprintf(stderr, "\nd_solver bbid: \n");
            d_solver.visit_vertices(vis_print_bbid);
            __DebugEnd;

            int re;

            // use-define analysis
            TimingBegin("u_solver.solve may");
            re =u_solver.solve<false>(use_gen, use_kill, use_meet, use_transfer);
            TimingEnd;
            assert(re == 0);

            // debug
            __DebugBegin(2);
            {
                std::string s = std::string("always_may_use_fg_") 
                    + std::to_string(ivl_process_lineno(net));
                u_solver.dump_graph(s.c_str());
            }
            __DebugEnd;

            // must-use analysis
            TimingBegin("u_solver.solve must");
            re = u_solver.solve<false>(null_gen, null_kill, use_meet_anticipate,
                                       use_transfer /*use set merge*/);
            TimingEnd;
            assert(re == 0);

            // debug
            __DebugBegin(2);
            {
                std::string s = std::string("always_must_use_fg_") 
                    + std::to_string(ivl_process_lineno(net));
                u_solver.dump_graph(s.c_str());
            }
            __DebugEnd;

            // analysis result
            TimingBegin("gen_up_result");
            gen_up_result(stmt, u_solver, d_solver, all_handle_l_signals, up_result);
            TimingEnd;
        }
        break;
    }
    case IVL_PR_INITIAL: {
        ivl_statement_t stmt = ivl_process_stmt(net); // first statement in process

        ud_solver_t d_solver;
        ud_solver_t u_solver;

        {
            int re = d_solver.create_cfg(stmt, 0);
            // assert(re == 0);
            if (re != 0)
                return re;

            __DebugBegin(3);
            auto vis_print_bbid = [](typename ud_solver_t::VertexDescriptor s,
                                     typename ud_solver_t::graph_t &g) -> int {
                fprintf(stderr, "[bbid=%u, stmt-no=%d]\n", g[s].bb_id,
                        g[s].bb ? (int)ivl_stmt_lineno(g[s].bb) : -1);
                return 0;
            };
            fprintf(stderr, "\nd_solver bbid: \n");
            d_solver.visit_vertices(vis_print_bbid);
            __DebugEnd;

            // dataflow solve
            re = d_solver.solve<true>(definition_gen, null_kill, definition_meet,
                                      definition_transfer);
            assert(re == 0);

            // debug
            __DebugBegin(2);
            //if(ivl_process_lineno(net)==133)
            {
                std::string s = std::string("init_may_def_fg_") 
                    + std::to_string(ivl_process_lineno(net));
                d_solver.dump_graph(s.c_str());
            }
            __DebugEnd;

            re = d_solver.solve<true>(null_gen, null_kill, definition_meet_anticipate,
                                      definition_transfer /*use set merge*/);
            assert(re == 0);

            // debug
            __DebugBegin(2);
            //if(ivl_process_lineno(net)==133)
            {
                std::string s = std::string("init_must_def_fg_") 
                    + std::to_string(ivl_process_lineno(net));
                d_solver.dump_graph(s.c_str());
            }
            __DebugEnd;

            // analysis result
            gen_down_result(stmt, d_solver, massign_result, up_result);
        }

        /* up analysis */
        {
            // int re = u_solver.create_cfg(stmt, 0);
            // assert(re == 0);
            u_solver.set_graph(d_solver.get_graph(), d_solver.get_entry(), d_solver.get_exit());
            u_solver.clear_dfa();

            int re;

            // dataflow solve
            re = u_solver.solve<false>(use_gen, use_kill, use_meet, use_transfer);
            assert(re == 0);

            // debug
            __DebugBegin(2);
            //if(ivl_process_lineno(net)==133)
            {
                std::string s = std::string("init_may_use_fg_") 
                    + std::to_string(ivl_process_lineno(net));
                u_solver.dump_graph(s.c_str());
            }
            __DebugEnd;

            re = u_solver.solve<false>(null_gen, null_kill, use_meet_anticipate,
                                       use_transfer /*use set merge*/);
            assert(re == 0);

            // debug
            __DebugBegin(2);
            //if(ivl_process_lineno(net)==133)
            {
                std::string s = std::string("init_must_use_fg_") 
                    + std::to_string(ivl_process_lineno(net));
                u_solver.dump_graph(s.c_str());
            }
            __DebugEnd;

            // analysis result
            gen_up_result(stmt, u_solver, d_solver, all_handle_l_signals, up_result);
        }

        break;
    }
    default:
        break;
    }

    return 0;
}

// nxz
// int analysis_sig_nxz(const ivl_process_t net, dfa_sig_nxz_t *p_sig_nxz) {
//     return 0;
// }

// clang-format on
