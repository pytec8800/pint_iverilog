#pragma once
#ifndef __DATAFLOW2_H__
#define __DATAFLOW2_H__
// clang-format off

//#define BOOST_ALLOW_DEPRECATED_HEADERS
#include "boost/graph/adjacency_list.hpp"
#include "boost/graph/graph_traits.hpp"
#include "boost/graph/breadth_first_search.hpp"

#include <unordered_map>
#include <functional>
#include <cstddef>
#include <type_traits>
#include <stdio.h>
#include <utility>
#include "common.h"
#include "dom_tree.h"

#include "priv.h"

#define SEQ_START (10)

// handle sub statement.
// warning: u->v may have parallel-edges!
#define HANDLE_SUBSTMT(sub, _edge, g, entry, exit, loop_depth)                         \
    do {                                                                               \
        auto _u = boost::source(_edge, g);                                             \
        auto _v = boost::target(_edge, g);                                             \
        assert((sub));                                                                 \
        auto stmt_type = ivl_statement_type(sub);                                      \
        if (IVL_ST_NOOP != stmt_type) {                                                \
            bool is_exist;                                                             \
            if (!((IVL_ST_BLOCK == stmt_type) && (0 == ivl_stmt_block_count(sub)))) {  \
                assert(IVL_ST_NONE != stmt_type);                                      \
                /* create new node*/                                                   \
                auto new_node = boost::add_vertex((g));                                \
                /* u->new; new->v;*/                                                   \
                std::tie(std::ignore, is_exist) = boost::add_edge((_u), new_node, (g));\
                assert(is_exist);                                                      \
                EdgeDescriptor new_v_edge;                                             \
                std::tie(new_v_edge, is_exist) = boost::add_edge(new_node, (_v), (g)); \
                assert(is_exist);                                                      \
                /* remove u->v edge */                                                 \
                boost::remove_edge((_edge), g);                                        \
                re = DFA_create_cfg<Graph, VertexProperty>(                            \
                    (sub), new_v_edge, (entry), (exit),                                \
                    (g), (loop_depth));                                                \
                if (re != 0)                                                           \
                    return re;                                                         \
            } else {                                                                   \
                /*do nothing*/                                                         \
            }                                                                          \
        }                                                                              \
    } while (false)

template <typename Graph, typename VertexProperty,
          typename VertexDescriptor = typename boost::graph_traits<Graph>::vertex_descriptor,
          typename EdgeDescriptor = typename boost::graph_traits<Graph>::edge_descriptor>
static int DFA_create_cfg(const ivl_statement_t net, EdgeDescriptor u_v_edge,
                          const VertexDescriptor &entry, const VertexDescriptor &exit, Graph &g,
                          unsigned int loop_depth = 0) {
    //=======================================================
    using InEdgeIter = typename boost::graph_traits<Graph>::in_edge_iterator;
    using OutEdgeIter = typename boost::graph_traits<Graph>::out_edge_iterator;
    //=======================================================

    const VertexDescriptor &u = boost::source(u_v_edge, g);
    const VertexDescriptor &v = boost::target(u_v_edge, g);

    // ERROR: the sequence is not working correctly
    static unsigned long long sequence = SEQ_START;

    // if(reset_seq){
    //    sequence = SEQ_START;
    //}

    const ivl_statement_type_t code = ivl_statement_type(net);
    int re = 0;

    switch (code) {
    case IVL_ST_ALLOC: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        re = -1;
        break;
    }

    case IVL_ST_ASSIGN:
    case IVL_ST_ASSIGN_NB: {
        VertexProperty &node_info = g[u];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;

        // if has # or @ at right side, then not support
        if (ivl_stmt_delay_expr(net)) {
            re = -1;
        }

        if (code == IVL_ST_ASSIGN_NB && (ivl_stmt_nevent(net) != 0)) {
            re = -1;
        }
        break;
    }

    case IVL_ST_BLOCK: {
        unsigned cnt = ivl_stmt_block_count(net);
        assert(cnt > 0); // BLOCK must contain at least one statement.
        bool is_exist;

        std::vector<std::pair<VertexDescriptor, VertexDescriptor>> saved_edges;

        // 1. extending 'cnt-1' nodes. Because current node store the first stmt
        // of
        // this block.
        VertexDescriptor s = u;
        // VertexDescriptor save1, save2;
        for (size_t i = 0; i < cnt - 1; i++) {
            auto new_node = boost::add_vertex(g);
            std::tie(std::ignore, is_exist) = boost::add_edge(s, new_node, g);
            assert(is_exist); // check inserted success

            // save edge. because node relationship may changed by rescusive
            // created.
            saved_edges.emplace_back(std::make_pair(s, new_node));
            s = new_node;
        }
        if (cnt > 1) {
            std::tie(std::ignore, is_exist) = boost::add_edge(s, v, g);
            assert(is_exist); // check inserted success
            saved_edges.emplace_back(std::make_pair(s, v));
            // boost::remove_edge(u, v, g); // remove u->v edge
            boost::remove_edge(u_v_edge, g); // remove u->v edge
        } else {
            saved_edges.emplace_back(std::make_pair(s, v));
        }

        // 2. handle sub stmts
        VertexDescriptor v1, v2;
        EdgeDescriptor v1_v2_edge;
        for (size_t i = 0; i < cnt; i++) {
            std::tie(v1, v2) = saved_edges[i];
            ivl_statement_t cur = ivl_stmt_block_stmt(net, i);
            ivl_statement_type_t cur_type = ivl_statement_type(cur);
            if ((IVL_ST_BLOCK == cur_type) || (IVL_ST_FORK == cur_type)) {
                // assert(ivl_stmt_block_count(cur) >0);
                if (ivl_stmt_block_count(cur) == 0) {
                    // delete node in this single chain
                    assert(boost::out_degree(v1, g) == 1);
                    InEdgeIter e_end, e_it, next;
                    std::tie(e_it, e_end) = boost::in_edges(v1, g);
                    for (next = e_it; e_it != e_end; e_it = next) {
                        next++;
                        auto prev = boost::source(*e_it, g);
                        std::tie(std::ignore, is_exist) = boost::add_edge(prev, v2, g);
                        assert(is_exist); // check inserted success
                        boost::remove_edge(prev, v1, g);
                    }
                    boost::remove_vertex(v1, g);
                    continue;
                }
            }
            std::tie(v1_v2_edge, is_exist) = boost::edge(v1, v2, g);
            assert(is_exist);
            re = DFA_create_cfg<Graph, VertexProperty>(cur, v1_v2_edge, entry, exit, g, loop_depth);
            if (re != 0)
                return re;
        }
        break;
    }

    case IVL_ST_CASEX:
    case IVL_ST_CASEZ:
    case IVL_ST_CASER:
    case IVL_ST_CASE: {
        ivl_expr_t ex = ivl_stmt_cond_expr(net);
        unsigned int count = ivl_stmt_case_count(net);

        VertexProperty &node_info = g[u];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;

        assert(ex);
        assert(boost::out_degree(u, g) >= 1);
        assert(boost::in_degree(v, g) >= 1);

        EdgeDescriptor branch_edge;
        bool is_exist;

        assert(count > 0);
        unsigned int default_case = count;
        for (unsigned int idx = 0; idx < count; idx++) {
            ivl_expr_t cex = ivl_stmt_case_expr(net, idx);
            if (cex) {
                ivl_statement_t cst = ivl_stmt_case_stmt(net, idx);

                assert(cst);
                if (cst) { // if this branch has statement
                    // add parallel edge 'u->v'
                    std::tie(branch_edge, is_exist) = boost::add_edge(u, v, g);
                    assert(is_exist);
                    HANDLE_SUBSTMT(cst, branch_edge, g, entry, exit, loop_depth);
                }
            } else {
                default_case = idx;
                // continue;
            }
        }

        // handle default_case and jump out. use 'u_v_edge'
        if (default_case != count) {
            assert(default_case < count);
            ivl_statement_t cst = ivl_stmt_case_stmt(net, default_case);
            assert(cst);
            if (cst) { // if this branch has statement
                HANDLE_SUBSTMT(cst, u_v_edge, g, entry, exit, loop_depth);
            }
        } else {
            // no default case. but the u_v_edge should also exist
        }
        break;
    }

    case IVL_ST_CASSIGN: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        re = -1;
        // show_stmt_cassign(net, ind);
        break;
    }

    case IVL_ST_CONDIT: {
        ivl_expr_t ex = ivl_stmt_cond_expr(net);
        ivl_statement_t t = ivl_stmt_cond_true(net);
        ivl_statement_t f = ivl_stmt_cond_false(net);

        VertexProperty &node_info = g[u];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;

        assert(ex);
        // NetAssert(0, net);
        assert(boost::out_degree(u, g) >= 1);
        assert(boost::in_degree(v, g) >= 1);

        EdgeDescriptor ture_edge, false_edge;
        bool is_exist;

        std::tie(ture_edge, is_exist) = boost::edge(u, v, g);
        assert(is_exist);

        // 1. add parallel edge 'u->v'
        std::tie(false_edge, is_exist) = boost::add_edge(u, v, g);
        assert(is_exist);

        // 2.
        // assert(t);
        if (t) {
            HANDLE_SUBSTMT(t, ture_edge, g, entry, exit, loop_depth);
        }

        if (f) {
            HANDLE_SUBSTMT(f, false_edge, g, entry, exit, loop_depth);
        }

        break;
    }

    case IVL_ST_CONTRIB: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // fprintf(out, "%*sCONTRIBUTION ( <+ )\n", ind, "");
        // show_expression(ivl_stmt_lexp(net), ind + 4);
        // show_expression(ivl_stmt_rval(net), ind + 4);
        break;
    }

    case IVL_ST_DEASSIGN: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // fprintf(out, "%*sDEASSIGN <lwidth=%u>\n", ind, "",
        // ivl_stmt_lwidth(net));
        // for (idx = 0; idx < ivl_stmt_lvals(net); idx += 1)
        //  show_assign_lval(ivl_stmt_lval(net, idx), ind + 4);
        break;
    }

    case IVL_ST_DISABLE: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // show_stmt_disable(net, ind);
        break;
    }

    case IVL_ST_DO_WHILE: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // fprintf(out, "%*sdo\n", ind, "");
        // show_statement(ivl_stmt_sub_stmt(net), ind + 2);
        // fprintf(out, "%*swhile\n", ind, "");
        // show_expression(ivl_stmt_cond_expr(net), ind + 4);
        break;
    }

    case IVL_ST_FORCE: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // show_stmt_force(net, ind);
        break;
    }

    case IVL_ST_FORK: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // unsigned cnt = ivl_stmt_block_count(net);
        // fprintf(out, "%*sfork\n", ind, "");
        // for (idx = 0; idx < cnt; idx += 1) {
        //  ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
        //  show_statement(cur, ind + 4);
        //}
        // fprintf(out, "%*sjoin\n", ind, "");
        break;
    }

    case IVL_ST_FORK_JOIN_ANY: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // unsigned cnt = ivl_stmt_block_count(net);
        // fprintf(out, "%*sfork\n", ind, "");
        // for (idx = 0; idx < cnt; idx += 1) {
        //  ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
        //  show_statement(cur, ind + 4);
        //}
        // fprintf(out, "%*sjoin_any\n", ind, "");
        break;
    }

    case IVL_ST_FORK_JOIN_NONE: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // unsigned cnt = ivl_stmt_block_count(net);
        // fprintf(out, "%*sfork\n", ind, "");
        // for (idx = 0; idx < cnt; idx += 1) {
        //  ivl_statement_t cur = ivl_stmt_block_stmt(net, idx);
        //  show_statement(cur, ind + 4);
        //}
        // fprintf(out, "%*sjoin_none\n", ind, "");
        break;
    }

    case IVL_ST_FREE: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // fprintf(out, "%*sfree automatic storage ...\n", ind, "");
        break;
    }

    case IVL_ST_NOOP: {
        VertexProperty &node_info = g[u];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;
        // node->set_type(CFG_NODE_NORMAL);
        // node->set_stmt(net);
        break;
    }

    case IVL_ST_RELEASE: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // show_stmt_release(net, ind);
        break;
    }

    case IVL_ST_STASK: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
        //VertexProperty &node_info = g[u];
        //node_info.bb = net;
        //node_info.bb_id = sequence;
        //node_info.loop_depth = loop_depth;
        //break;
        // fprintf(out, "%*sCall %s(%u parameters); /* %s:%u */\n", ind, "",
        //        ivl_stmt_name(net), ivl_stmt_parm_count(net),
        //        ivl_stmt_file(net),
        //        ivl_stmt_lineno(net));
        // for (idx = 0; idx < ivl_stmt_parm_count(net); idx += 1)
        //  if (ivl_stmt_parm(net, idx))
        //    show_expression(ivl_stmt_parm(net, idx), ind + 4);
        //break;
    }

    case IVL_ST_TRIGGER: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // show_stmt_trigger(net, ind);
        break;
    }

    case IVL_ST_UTASK: {
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_DELAY:
    case IVL_ST_DELAYX:
    case IVL_ST_WAIT: {
        bool is_exist;

        // 1. fill wait node info
        VertexProperty &node_info = g[u];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;

        // 2. remove all out-edge of vertex `u`
        OutEdgeIter e_it, e_end, next;
        std::tie(e_it, e_end) = boost::out_edges(u, g);
        for (next = e_it; e_it != e_end; e_it = next) {
            next++;
            boost::remove_edge(e_it, g);
        }

        // 3. let `u` connect to exit node
        std::tie(std::ignore, is_exist) = boost::add_edge(u, exit, g);
        assert(is_exist); // check inserted success

        // 4. let entry connect to `v`
        EdgeDescriptor entry_v_edge;
        std::tie(entry_v_edge, is_exist) = boost::add_edge(entry, v, g);
        assert(is_exist); // check inserted success

        // 5. handle sub stmt with entry->v edge
        ivl_statement_t sub = ivl_stmt_sub_stmt(net);
        HANDLE_SUBSTMT(sub, entry_v_edge, g, entry, exit, loop_depth);

        // assert(sub);
        // assert(
        //  ivl_statement_type(sub)==IVL_ST_BLOCK ||
        //  ivl_statement_type(sub)==IVL_ST_NOOP);
        // DFA_create_cfg<Graph, VertexProperty>(sub, entry, v, entry, exit, g,
        // loop_depth);
        break;
    }

    case IVL_ST_REPEAT:
    case IVL_ST_WHILE: {
        loop_depth++; // increase the depth of loop nesting
        bool is_exist;

        // 1. handle while condition node
        VertexProperty &node_info = g[u];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;
        EdgeDescriptor u_u_edge;
        std::tie(u_u_edge, is_exist) = boost::add_edge(u, u, g); // make a loop
        assert(is_exist);

        // 2. handle loop body
        ivl_statement_t sub = ivl_stmt_sub_stmt(net);
        assert(sub);
        // assert(
        //  ivl_statement_type(sub)==IVL_ST_BLOCK ||
        //  ivl_statement_type(sub)==IVL_ST_NOOP); // [zyc][why?]

        HANDLE_SUBSTMT(sub, u_u_edge, g, entry, exit, loop_depth);
        // DFA_create_cfg<Graph, VertexProperty>(sub, u, u, entry, exit, g,
        // loop_depth);

        loop_depth--; // exit loop
        break;
    }

    case IVL_ST_FOREVER:{
        re = -1;
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_NONE: { // unreachable
        re = -1;
        NetAssertWrongPath(net);
        break;
    }

    default: {
        re = -1;
        NetAssertWrongPath(net);
    }
    }

    sequence++;
    return re;
}

//// new dataflow arch
//============================================================================================

struct fg_node_s{
    ivl_statement_t bb{nullptr};
    size_t bb_id{0};
};

class flow_graph_helper_t{
    using cfg_t = boost::adjacency_list<
        boost::vecS, // Allowing parallel edges. should delete parallel edges finally
        boost::vecS,
        boost::bidirectionalS,
        fg_node_s,
        boost::no_property,
        boost::no_property,
        boost::listS>;
    using CfgVertex = typename boost::graph_traits<cfg_t>::vertex_descriptor;
    using CfgEdge = typename boost::graph_traits<cfg_t>::edge_descriptor;
    using CfgOutEdgeIter = typename boost::graph_traits<cfg_t>::out_edge_iterator;
    using CfgInEdgeIter = typename boost::graph_traits<cfg_t>::in_edge_iterator;
    using CfgVertexIter = typename boost::graph_traits<cfg_t>::vertex_iterator;

  public:
    flow_graph_helper_t(ivl_statement_t net, ivl_process_type proc_type)
    {
        int re;
        re = create_cfg(net, proc_type);
        assert(re==0);
        
        m_domtree.make_domtree(m_cfg, m_cfg_entry);
    }
    
    int create_cfg(ivl_statement_t net, ivl_process_type proc_type)
    {
        return 0;
    }
    
    // visit all vertices.(include all sub-graphs)
    // `vis` should not modify the vertices and edges
    // "int vis(const VertexDescriptor&, const graph_t&);"
    template <typename V> 
    void visit_vertices(V &&vis) {
        assert(!m_vertex_array.empty());
        for (const auto &s : m_vertex_array) {
            int re = vis(s, m_graph);
            assert(re == 0);
        }
    }

    template <typename V> 
    void visit_vertices_reverse(V &&vis) {
        assert(!m_vertex_array.empty());
        auto it = m_vertex_array.rbegin();
        for (; it != m_vertex_array.rend(); it++) {
            int re = vis(*it, m_graph);
            assert(re == 0);
        }
    }

    size_t get_vertices_num() {
        return (size_t)boost::num_vertices(m_graph);
    }
    
    std::shared_ptr<cfg_t> get_cfg(){
        ;
    }

    CfgVertex get_entry() { return m_cfg_entry; }

    CfgVertex get_exit() { return m_cfg_exit; }
    
    void dump_cfg(const char *name) {
        FILE *p_file = fopen((std::string(name) + ".dot").c_str(), "w");
        VertexIter v_it, v_end;

        auto vis_dump = [&p_file](const VertexDescriptor &s, const graph_t &g) -> int {
            auto vp = g[s];
            // print vertex
            // assert(vp.bb);
            std::string label_content;
            label_content.reserve(256);
            /*
              format:
              in:\n
              a[1,2,3,],\n
              b[1,],\n
            */
            label_content.append("in:\n");
            for (const auto &defs : vp.in) {
                auto sig = defs.m_sig;
                const char *sig_name = ivl_signal_basename(sig);

                label_content.append(sig_name);
                label_content.append("[");

                for (const auto &stmt : defs.m_stmts) {
                    assert(stmt);
                    unsigned int lineno = ivl_stmt_lineno(stmt);

                    label_content.append(std::to_string(lineno));
                    label_content.append(",");
                }

                label_content.append("],\n");
            }
            /*
              format:
              gen:\n
              a[1,2,3,],\n
              b[1,],\n
            */
            label_content.append("gen:\n");
            for (const auto &defs : vp.gen) {
                auto sig = defs.m_sig;
                const char *sig_name = ivl_signal_basename(sig);

                label_content.append(sig_name);
                label_content.append("[");

                for (const auto &stmt : defs.m_stmts) {
                    assert(stmt);
                    unsigned int lineno = ivl_stmt_lineno(stmt);

                    label_content.append(std::to_string(lineno));
                    label_content.append(",");
                }

                label_content.append("],\n");
            }
            /*
              format:
                out:\n
                a[1,2,3],\n
                b[2,3],\n
            */
            label_content.append("out:\n");
            for (const auto &out_defs : vp.out) {
                auto sig = out_defs.m_sig;
                const char *sig_name = ivl_signal_basename(sig);

                label_content.append(sig_name);
                label_content.append("[");

                for (const auto &stmt : out_defs.m_stmts) {
                    assert(stmt);
                    unsigned int lineno = ivl_stmt_lineno(stmt);

                    label_content.append(std::to_string(lineno));
                    label_content.append(",");
                }

                label_content.append("],\n");
            }

            // fprintf(p_file, "V%lld [label=\"lineno:%d\"];\n",
            //  (long long)s,
            //  vp.bb==nullptr ? -1 : (int)ivl_stmt_lineno(vp.bb));
            if (vp.bb_id <= 2) {
                fprintf(p_file, "V%lld [label=\"%s\n%s\"];\n", (long long)s,
                        vp.bb_id == 1 ? "Entry" : "Exit", label_content.c_str());
            } else {
                assert(vp.bb);
                fprintf(p_file, "V%lld [label=\"lineno=%d\n%s\"];\n", (long long)s,
                        (int)ivl_stmt_lineno(vp.bb), label_content.c_str());
            }

            // print out edge
            OutEdgeIter e_it, e_end;
            for (std::tie(e_it, e_end) = boost::out_edges(s, g); e_it != e_end; e_it++) {
                auto v = boost::target(*e_it, g);
                fprintf(p_file, "V%lld->V%lld;\n", (long long)s, (long long)v);
            }
            return 0;
        };

        fprintf(p_file, "digraph %s {\n", name);
        visit_vertices(vis_dump);
        fprintf(p_file, "}\n");
        fclose(p_file);
    }

    /* domtree operation */
    
    // compute lowest common ancestor
    CfgVertex domtree_lca(const std::vector<CfgVertex>& v1_vec)
    {
        assert(!v1_vec.empty());

        std::vector<Vertex3> v3_vec;
        for(const auto& v1: v1_vec){
            v3_vec.emplace_back(m_v1v3_map[v1]);
        }
        
        // find min depth vertex
        Vertex3 min_depth_v = *std::min_element(
            v3_vec.begin(), v3_vec.end(), 
            [this](const Vertex3& l, const Vertex3& r) -> bool{
                return this->m_v3_depth[l] < this->m_v3_depth[r];
            });
        size_t min_depth = m_v3_depth[min_depth_v];
        
        // up all v3 to the same depth
        for(auto& v3: v3_vec){
            size_t depth_distance = m_v3_depth[v3] - min_depth;
            for(size_t i=0; i<depth_distance; i++){
                v3 = get_idom(v3);
            }
        }
        
        // find command ancestor
        bool is_all_same = std::all_of(v3_vec.begin(), v3_vec.end(), 
            [&v3_vec, this](const Vertex3& v) -> bool{
                assert(this->m_v3_depth[v] == this->m_v3_depth[v3_vec[0]]);
                return v==v3_vec[0]; // v3_vec must non-empty!
            });
        while(!is_all_same){
            // up the depth of all nodes at the same time
            for(auto& v3: v3_vec){
                v3 = get_idom(v3);
            }

            // check if all nodes are the same node
            is_all_same = std::all_of(v3_vec.begin(), v3_vec.end(), 
                [&v3_vec, this](const Vertex3& v) -> bool{
                    assert(this->m_v3_depth[v] == this->m_v3_depth[v3_vec[0]]);
                    return v==v3_vec[0]; // v3_vec must non-empty!
                });
        }

        return m_v3v1_map[ v3_vec[0] ];
    }

    // return true, if "a dom b" satisfied. (a is dominator)
    bool is_dom(CfgVertex a, CfgVertex b){
        auto ancestor = domtree_lca({a, b});
        return (ancestor==a);
    }

    void dump_domtree(const char* name, const Graph1& cfg){
        auto vertex_printer = [&cfg, this](
            FILE* p_file, Vertex3 s, const Graph3 &g) -> void
        {
            ivl_statement_t stmt = cfg[m_v3v1_map[s]].bb; // cfg must have bb property
            int lineno = stmt ? (int)ivl_stmt_lineno(stmt) : -1;
            fprintf(p_file, "V%lld [label=\"line:%d\"];\n", (long long)s, lineno);
        };
        dump_digraph(name, m_domTree, vertex_printer);
    }

  private:
    // flow graph info
    CfgVertex m_cfg_entry;
    CfgVertex m_cfg_exit;
    std::shared_ptr<cfg_t> m_cfg;
    std::vector<CfgVertex> m_cfg_vertices;
    
    // dom tree info
    std::shared_ptr<domtree_t<cfg_t>> m_domtree;
};

template <typename T, // flow value type
          typename TCompare>
struct fv_node_s {
    using FvSet = std::set<T, TCompare>;
    using FvSetRef = std::set<T, TCompare> &;
    using FvSetConstRef = const std::set<T, TCompare> &;
    
    std::set<T, TCompare> in;
    std::set<T, TCompare> out;
    std::set<T, TCompare> gen;
    std::set<T, TCompare> kill;
};

template<typename CfgGraph, 
         typename T, 
         typename TCompare = std::less<T>>
class dfa_solver_t
{
  public:
    using CfgVertex = typename boost::graph_traits<CfgGraph>::vertex_descriptor;
    using CfgVConstIter = typename std::vector<CfgVertex>::const_iterator;

    using FvNode_t = fv_node_s<T, TCompare>;
    using FvSet = typename FvNode_t::FvSet;
    using FvSetRef = typename FvNode_t::FvSetRef;
    using FvSetConstRef = typename FvNode_t::FvSetConstRef;

    using VFvMap_t = std::unordered_map<CfgVertex, FvNode_t>;

  public:
    dfa_solver_t(
        std::shared_ptr<CfgGraph> p_cfg, 
        CfgVConstIter vertex_begin,
        CfgVConstIter vertex_end)
    {
        assert(p_cfg);
        m_cfg = p_cfg;

        m_cfg_vertex_begin = vertex_begin;
        m_cfg_vertex_end = vertex_end;
        assert(std::distance(m_cfg_vertex_begin, m_cfg_vertex_end)>0);

        m_v_fv_map = std::make_shared<VFvMap_t>();
        assert(m_v_fv_map);
    }

    template <typename GenGenerator, typename KillGenerator>
    int gen_kill(GenKillOp&& fn_genkill) 
    {
        VFvMap_t& vfv_map = *m_v_fv_map;
        auto vis_vertex = [&fn_genkill, &vfv_map](
            CfgVertex u, const CfgGraph &g) -> int 
        {
            if (g[u].bb) {
                FvSetRef out_gen = vfv_map[u].gen;
                FvSetRef out_kill = vfv_map[u].kill;
                int re = fn_genkill(out_gen, out_kill, g[u].bb);
                assert(0 == re);
            }
            return 0;
        };
        visit_vertices(vis_vertex);
        return 0;
    }
    
    template<typename Visitor>
    void visit_vertices(Visitor&& vis)
    {
        assert(std::distance(m_cfg_vertex_begin, m_cfg_vertex_end)>0);
        for (CfgVConstIter vi=m_cfg_vertex_begin; vi!=m_cfg_vertex_end; vi++)
        {
            int re = vis(*vi, *m_cfg);
            assert(re == 0);
        }
    }

    template<typename Visitor>
    void visit_vertices_reverse(Visitor&& vis)
    {
        assert(std::distance(m_cfg_vertex_begin, m_cfg_vertex_end)>0);
        CfgVConstIter r_vi = std::advance(m_cfg_vertex_end, -1);
        CfgVConstIter r_end = std::advance(m_cfg_vertex_begin, -1);
        for (; r_vi!=r_end; r_vi--)
        {
            int re = vis(*r_vi, *m_cfg);
            assert(re == 0);
        }
    }

    // Assume Gen/kill has been computed.
    template <bool Forward, typename MeetOp, typename TransferOp>
    int solve_fixed_point(
        VertexDescriptor UNUSED(s), 
        MeetOp&& meet_op, 
        TransferOp&& transfer_op)
    {
    write here............

        bool changed = true;
        bool is_changed = false;
        std::vector<bool> changed_vec;

        DFA_solve_visitor<Forward, T, TCompare, MeetOp, TransferOp> 
            vis(meet_op, transfer_op, is_changed, changed_vec);

        while (changed == true) {
            vis.m_is_changed = false;
            vis.m_is_changed_vec.clear();

            auto vis_vertex = [&vis](VertexDescriptor s, graph_t &g) -> int {
                vis.discover_vertex(s, g);
                return 0;
            };
            if (Forward) {
                visit_vertices(vis_vertex);
            } else { // backward
                visit_vertices_reverse(vis_vertex);
            }
            changed = vis.m_is_changed;
            //__DebugBegin(1);
            // printf("\nchanged = %s\n", changed ? "true" : "false");
            // std::sprintf(i_str, "solver_iterate_%d", i++);
            // dump_graph(i_str);
            // fprintf(stderr, "is_changed_vec size=%lu\n",
            //        (unsigned long)vis.m_is_changed_vec->size());
            // for (auto b : *vis.m_is_changed_vec) {
            //    fprintf(stderr, "is_changed_vec=%s\n", b ? "true" : "false");
            //}
            //__DebugEnd;
        }

        return 0;
    }

    // this function may be called multiple times.
    template <
        bool Forward, typename GenOp, typename KillOp, 
        typename MeetOp, typename TransferOp>
    int solve(GenOp &&gen_op, KillOp &&kill_op, MeetOp &&meet_op, TransferOp &&transfer_op) {
        int re;
        // 1. compute gen/kill. Dont modify in/out
        re = gen_kill(m_entry, std::forward<GenOp>(gen_op), std::forward<KillOp>(kill_op));
        assert(0 == re);

        __DebugBegin(1);
        dump_graph("gen_kill");
        __DebugEnd;

        // 2. solve DFA equations. Dont modify gen/kill
        re = solve_fixed_point<Forward>(m_entry, std::forward<MeetOp>(meet_op),
                                        std::forward<TransferOp>(transfer_op));
        assert(0 == re);

        return 0;
    }

  private:
    std::shared_ptr<CfgGraph> m_cfg;
    typename std::vector<CfgVertex>::const_iterator m_cfg_vertex_begin;
    typename std::vector<CfgVertex>::const_iterator m_cfg_vertex_end;

    // cfg vertex -> flow value mapping
    std::shared_ptr<VFvMap_t> m_v_fv_map;
};


#endif
// clang-format on
