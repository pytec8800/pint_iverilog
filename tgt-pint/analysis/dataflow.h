#pragma once
#ifndef __DATAFLOW_H__
#define __DATAFLOW_H__
// clang-format off

//#define BOOST_ALLOW_DEPRECATED_HEADERS
#include "boost/graph/adjacency_list.hpp"
#include "boost/graph/graph_traits.hpp"
#include "boost/graph/breadth_first_search.hpp"

#include "priv.h"

#include <unordered_map>
#include <functional>
#include <cstddef>
#include <type_traits>
#include <stdio.h>
#include <utility>
#include "common.h"

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

//// DFA solver implementation
//============================================================================================
struct no_operator_t {};

template <typename T, typename TCompare, typename GenOp, typename KillOp>
class DFA_genkill_visitor : public boost::default_bfs_visitor {
  public:
    using FvSetRef = std::set<T, TCompare> &;

    GenOp gen_op;
    KillOp kill_op;

  public:
    DFA_genkill_visitor(GenOp &&genop, KillOp &&killop) : gen_op(genop), kill_op(killop) {}

    template <typename Vertex, typename Graph> void discover_vertex(Vertex u, const Graph &g) {
        int re = 0;
        // 1. compute gen
        // if (!std::is_same<no_operator_t, GenOp>::value){
        FvSetRef out_gen = const_cast<FvSetRef>(g[u].gen);
        if (g[u].bb) {
            re = gen_op(out_gen, g[u].bb);
            assert(0 == re);
        }
        //}

        // 2. compute kill
        // if (!std::is_same<no_operator_t, KillOp>::value){
        FvSetRef out_kill = const_cast<FvSetRef>(g[u].kill);
        if (g[u].bb) {
            re = kill_op(out_kill, g[u].bb);
            assert(0 == re);
        }
        //}
    }
};

template <bool Forward, typename T, typename TCompare, typename MeetOp, typename TransferOp>
class DFA_solve_visitor : public boost::default_bfs_visitor {
  public:
    using FvSet = std::set<T, TCompare>;
    using FvSetRef = std::set<T, TCompare> &;
    using FvSetConstRef = const std::set<T, TCompare> &;

    MeetOp &meet_op;
    TransferOp &transfer_op;

    // bool *m_is_changed = nullptr;
    // std::vector<bool> *m_is_changed_vec = nullptr;
    bool &m_is_changed;
    std::vector<bool> &m_is_changed_vec;

  public:
    explicit DFA_solve_visitor(MeetOp &&meetop, TransferOp &&transferop, bool &is_changed,
                               std::vector<bool> &is_changed_vec)
        : meet_op(std::forward<MeetOp>(meetop)), transfer_op(std::forward<TransferOp>(transferop)),
          m_is_changed(is_changed), m_is_changed_vec(is_changed_vec) {
        // fprintf(stderr, "construct line:%d. is_changed=%s\n",
        //  __LINE__, is_changed?"true":"false");
    }

    template <typename Vertex, typename Graph> void discover_vertex(Vertex u, const Graph &g) {
        using InEdgeIter = typename boost::graph_traits<Graph>::in_edge_iterator;
        using OutEdgeIter = typename boost::graph_traits<Graph>::out_edge_iterator;

        int re;

        if (Forward) {
            // 0. bakup out-state.
            FvSet bak_out = g[u].out;
            
            // 1. compute In[B] by using meet_op.
            FvSet tmp;
            InEdgeIter e_it, e_begin, e_it_end;
            std::tie(e_begin, e_it_end) = boost::in_edges(u, g);
            for (e_it = e_begin; e_it != e_it_end; e_it++) {
                if (e_it != e_begin) {
                    auto prev_node = boost::source(*e_it, g);
                    
                    //__DBegin(==10);
                    //{
                    //    if(g[prev_node].bb)
                    //        fprintf(stderr, "\n-----------meet_op prev-node-stmtlineno:%u\n",
                    //            ivl_stmt_lineno(g[prev_node].bb));
                    //}
                    //__DEnd;
                    
                    re = meet_op(tmp, tmp, g[prev_node].out);
                    
                    //__DBegin(==10);
                    //{
                    //    if(g[prev_node].bb)
                    //        fprintf(stderr, "\n----------------------------------------\n");
                    //}
                    //__DEnd;
                    
                    assert(re == 0);
                } else {
                    auto prev_node = boost::source(*e_it, g);
                    tmp = g[prev_node].out;
                }
            }
            FvSetRef u_in = const_cast<FvSetRef>(g[u].in);
            u_in = std::move(tmp);

            // 2. compute Out[B] by using transfer_op.
            // if (!std::is_same<no_operator_t, TransferOp>::value){
            //__DBegin(==10);
            //{
            //    if(g[u].bb)
            //        fprintf(stderr, "\n-----------------transfer_op stmtlineno:%u\n",
            //            ivl_stmt_lineno(g[u].bb));
            //}
            //__DEnd;
            re = transfer_op(const_cast<FvSetRef>(g[u].out), g[u].in, g[u].gen, g[u].kill);
            assert(re == 0);
            //__DBegin(==10);
            //{
            //    if(g[u].bb)
            //        fprintf(stderr, "\n----------------------------------------\n");
            //}
            //__DEnd;
            //}

            // `==` means complete equal
            if (!(bak_out == g[u].out)) {
                // fprintf(stderr, "line:%d\n", __LINE__);
                m_is_changed = true;
            } else {
                // fprintf(stderr, "line:%d\n", __LINE__);
            }
            m_is_changed_vec.push_back(
                m_is_changed); // c++11 only support push_back for vector<bool>
        } else {
            // 0. bakup out-state.
            // FvSet bak_out = g[u].out;
            FvSet bak_in = g[u].in;

            // 1. compute In[B] by using meet_op.
            FvSet tmp;
            OutEdgeIter e_it, e_begin, e_it_end;
            std::tie(e_begin, e_it_end) = boost::out_edges(u, g);
            for (e_it = e_begin; e_it != e_it_end; e_it++) {
                if (e_it != e_begin) {
                    auto succ_node = boost::target(*e_it, g);
                    re = meet_op(tmp, tmp, g[succ_node].in);
                    assert(re == 0);
                } else {
                    auto succ_node = boost::target(*e_it, g);
                    tmp = g[succ_node].in;
                }
            }
            FvSetRef u_out = const_cast<FvSetRef>(g[u].out);
            u_out = std::move(tmp);

            // 2. compute Out[B] by using transfer_op.
            re = transfer_op(const_cast<FvSetRef>(g[u].in), g[u].out, g[u].gen, g[u].kill);
            assert(re == 0);

            // `==` means complete equal
            if (!(bak_in == g[u].in)) {
                // fprintf(stderr, "line:%d\n", __LINE__);
                m_is_changed = true;
            } else {
                // fprintf(stderr, "line:%d\n", __LINE__);
            }
            m_is_changed_vec.push_back(
                m_is_changed); // c++11 only support push_back for vector<bool>
        }
    }
};

// save the BFS traversal order. (for forward/backward iterate)
template <typename VertexVec> class bfs_order_visitor_s : public boost::default_bfs_visitor {
  public:
    explicit bfs_order_visitor_s(VertexVec &v) : m_vertex_array(v) {}
    template <typename Vertex, typename Graph> void discover_vertex(Vertex u, const Graph &) {
        m_vertex_array.emplace_back(u);
    }
    VertexVec &m_vertex_array;
};

template <typename BasicBlock,
          typename T, // flow value type
          typename TCompare>
struct DFA_node_s {
    using FvSet = std::set<T, TCompare>;
    using FvSetRef = std::set<T, TCompare> &;
    using FvSetConstRef = const std::set<T, TCompare> &;
    // dfa info
    std::set<T, TCompare> in;
    std::set<T, TCompare> out;
    std::set<T, TCompare> gen;
    std::set<T, TCompare> kill;
    // cfg info
    BasicBlock bb{nullptr};
    unsigned int bb_id{0};  // sequence
    unsigned loop_depth{0}; // =0: no loop; >0: loop depth;
};

template <
    typename BasicBlock, 
    typename T, 
    typename TCompare = std::less<T> >
class DFA_solver_t {
  public:
    //// typedef [begin]
    using fv_type = T; // flow value type
    using node_property_t = DFA_node_s<BasicBlock, T, TCompare>;
    using basic_block_t = BasicBlock;
    using graph_t = boost::adjacency_list<
/*class OutEdgeListS =   */ boost::multisetS,   // use 'listS'/'multisetS' for allowing
                                                // parallel edges
/*class VertexListS =    */ boost::vecS, // listS for 'lengauer_tarjan_dominator_tree' algorithm
/*class DirectedS =      */ boost::bidirectionalS,
/*class VertexProperty = */ node_property_t,
/*class EdgeProperty =   */ boost::no_property,
/*class GraphProperty =  */ boost::no_property,
/*class EdgeListS =      */ boost::listS>;
    using VertexDescriptor = typename boost::graph_traits<graph_t>::vertex_descriptor;
    using EdgeDescriptor = typename boost::graph_traits<graph_t>::edge_descriptor;
    using OutEdgeIter = typename boost::graph_traits<graph_t>::out_edge_iterator;
    using InEdgeIter = typename boost::graph_traits<graph_t>::in_edge_iterator;
    using VertexIter = typename boost::graph_traits<graph_t>::vertex_iterator;
    //// typedef [end]

    template <typename GenGenerator, typename KillGenerator>
    int gen_kill(
        VertexDescriptor UNUSED(s), 
        GenGenerator&& fn_gen, 
        KillGenerator&& fn_kill) 
    {
        DFA_genkill_visitor<T, TCompare, GenGenerator, KillGenerator> vis(fn_gen, fn_kill);
        // boost::breadth_first_search(m_graph, s, boost::visitor(vis));
        auto vis_vertex = [&vis](VertexDescriptor s, graph_t &g) -> int {
            vis.discover_vertex(s, g);
            return 0;
        };
        visit_vertices(vis_vertex);
        return 0;
    }

    // Assume Gen/kill has been computed.
    template <bool Forward, typename MeetOp, typename TransferOp>
    int solve_fixed_point(
        VertexDescriptor UNUSED(s), 
        MeetOp&& meet_op, 
        TransferOp&& transfer_op)
    {
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
            //return 0;
            //__DBegin(==10);
            //{
            //    static size_t i=0;
            //    std::string str = 
            //        std::string("solver_iterate_") + std::to_string(i);
            //    //dump_graph(str.c_str(), assigncnt_v_printer<graph_t>);
            //}
            //__DEnd;

            //__DBegin(==10);
            //{
            //    fprintf("\nchanged = %s\n", changed ? "true" : "false");
            //    std::sprintf(i_str, "solver_iterate_%d", i++);
            //    dump_graph(i_str);
            //    fprintf(stderr, "is_changed_vec size=%lu\n",
            //        (unsigned long)vis.m_is_changed_vec->size());
            //    for (auto b : *vis.m_is_changed_vec) {
            //        fprintf(stderr, "is_changed_vec=%s\n", b ? "true" : "false");
            //    }
            //}
            //__DEnd;
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

    // template<typename AstNode>
    int create_cfg(const BasicBlock &net, int process_type = 0) {
        clear_cfg();

        VertexDescriptor new_node;
        EdgeDescriptor new_exit_edge;
        bool is_exist;
        int re = 0;

        m_entry = boost::add_vertex(m_graph);
        m_graph[m_entry].bb_id = 1; // bb_id==1 => entry node
        m_exit = boost::add_vertex(m_graph);
        m_graph[m_exit].bb_id = 2; // bb_id==2 => exit node

        new_node = boost::add_vertex(m_graph);
        // node_property_t& vp = m_graph[new_node];

        //if (process_type == 0) { // initial block
            std::tie(std::ignore, is_exist) = boost::add_edge(m_entry, new_node, m_graph);
            assert(is_exist);
            std::tie(new_exit_edge, is_exist) = boost::add_edge(new_node, m_exit, m_graph);
            assert(is_exist);

            re = DFA_create_cfg<graph_t, node_property_t>(net, new_exit_edge, m_entry, m_exit,
                                                          m_graph, 0);
            // assert(re == 0);
            if (re != 0)
                return re;
        //} else { // always block
        //    std::tie(std::ignore, is_exist) = boost::add_edge(m_entry, new_node, m_graph);
        //    assert(is_exist);
        //    std::tie(new_exit_edge, is_exist) = boost::add_edge(new_node, m_exit, m_graph);
        //    assert(is_exist);

        //    // make loop
        //    EdgeDescriptor new_new_edge;
        //    std::tie(new_new_edge, is_exist) = boost::add_edge(new_node, new_node, m_graph);
        //    assert(is_exist);

        //    re = DFA_create_cfg<graph_t, node_property_t>(net, new_new_edge, m_entry, m_exit,
        //                                                  m_graph, 0);
        //    // assert(re == 0);
        //    if (re != 0)
        //        return re;
        //}

        // save the BFS traversal order. (for forward/backward iterate)
        assert(m_vertex_array.empty());
        bfs_order_visitor_s<std::vector<VertexDescriptor>> vis(m_vertex_array);
        boost::breadth_first_search(m_graph, m_entry, boost::visitor(vis));

        // give vertex serial number
        unsigned int seq = SEQ_START;
        visit_vertices([&](VertexDescriptor s, graph_t &g) -> int {
            if (!((this->get_entry() == s) || (this->get_exit() == s))) {
                g[s].bb_id = seq;
                seq++;
            }
            return 0;
        });

        return 0;
    }

    void clear_cfg() {
        m_graph.clear();
        m_entry = m_exit = 0;
    }

    //// graph operation [begin]
    void set_graph(const graph_t &other, VertexDescriptor entry, VertexDescriptor exit) {
        //m_entry = entry;
        //m_exit = exit;
        //m_graph = other;
    
        //// save the BFS traversal order. (for forward/backward iterate)
        //assert(m_vertex_array.empty());
        //bfs_order_visitor_s<std::vector<VertexDescriptor>> vis(m_vertex_array);
        //boost::breadth_first_search(m_graph, m_entry, boost::visitor(vis));

        using Vertex1 = typename boost::graph_traits<graph_t>::vertex_descriptor;
        using Vertex2 = VertexDescriptor;

        std::unordered_map<Vertex2, Vertex1> v2v1_map;
        std::unordered_map<Vertex1, Vertex2> v1v2_map;

        set_graph<graph_t, Vertex1, Vertex2>(
            other, entry, exit, v2v1_map, v1v2_map);
    }
    
    template<
        typename Graph1, 
        typename Vertex1 =
            typename boost::graph_traits<Graph1>::vertex_descriptor,
        typename Vertex2 = VertexDescriptor>
    void set_graph(
        const Graph1 &other, 
        Vertex1 entry_other, 
        Vertex1 exit_other,
        std::unordered_map<
            Vertex2, 
            Vertex1>& v2v1_map,
        std::unordered_map<
            Vertex1, 
            Vertex2>& v1v2_map) 
    {
        auto vertex_copy = [&v2v1_map, &v1v2_map, &other, this]
            (Vertex1 old_v, Vertex2 new_v) -> void 
        {
            v2v1_map[new_v] = old_v;
            v1v2_map[old_v] = new_v;

            m_graph[new_v].bb = other[old_v].bb;
            m_graph[new_v].bb_id = other[old_v].bb_id;
            m_graph[new_v].loop_depth = other[old_v].loop_depth;
        };
        graph_relation_copy(other, m_graph, vertex_copy);

        m_entry = v1v2_map[entry_other];
        m_exit = v1v2_map[exit_other];

        // save the BFS traversal order. (for forward/backward iterate)
        assert(m_vertex_array.empty());
        bfs_order_visitor_s<std::vector<Vertex2>> vis(m_vertex_array);
        boost::breadth_first_search(m_graph, m_entry, boost::visitor(vis));
    }

    // clear dfa info, but hold on cfg info
    void clear_dfa() {
        auto vis_clear_vp = [](VertexDescriptor s, graph_t &g) -> int {
            g[s].in.clear();
            g[s].out.clear();
            g[s].gen.clear();
            g[s].kill.clear();
            return 0;
        };
        visit_vertices(vis_clear_vp);
    }

    graph_t clone_graph() { return m_graph; }

    graph_t &get_graph() { return m_graph; }

    VertexDescriptor get_entry() { return m_entry; }

    VertexDescriptor get_exit() { return m_exit; }

    // visit all vertices.(include all sub-graphs)
    // `vis` should not modify the vertices and edges
    // "int vis(const VertexDescriptor&, const graph_t&);"
    template <typename V> void visit_vertices(V &&vis) {
        assert(!m_vertex_array.empty());
        for (const auto &s : m_vertex_array) {
            int re = vis(s, m_graph);
            assert(re == 0);
        }
    }

    template <typename V> void visit_vertices_reverse(V &&vis) {
        assert(!m_vertex_array.empty());
        auto it = m_vertex_array.rbegin();
        for (; it != m_vertex_array.rend(); it++) {
            int re = vis(*it, m_graph);
            assert(re == 0);
        }
    }

    // print stmt line number only
    void dump_graph(const char *name)
    {
        auto v_printer = [this](FILE* p_file, const VertexDescriptor &s, const graph_t &g)->void
        {
            auto vp = g[s];
            if (s==m_entry || s==m_exit) {
                const char *str = s==m_entry ? "Entry" : "Exit";
                fprintf(p_file, "V%lld [label=\"%s\n\"];\n", (long long)s, str);
            } else {
                assert(vp.bb);
                fprintf(p_file, "V%lld [label=\"lineno=%d\n\"];\n", (long long)s,
                        (int)ivl_stmt_lineno(vp.bb));
            }
        };
        dump_graph(name, v_printer);
    }
    
    // do custom print
    template<typename VertexPrinter>
    void dump_graph(const char *name, VertexPrinter&& v_printer)
    {
        FILE *p_file = fopen((std::string(name) + ".dot").c_str(), "w");
        dump_digraph(name, m_graph, v_printer);
        fclose(p_file);
    }

    typename boost::graph_traits<graph_t>::vertices_size_type get_vertices_num() {
        return boost::num_vertices(m_graph);
    }
    //// graph operation [end]

  private:
    graph_t m_graph;
    VertexDescriptor m_entry;
    VertexDescriptor m_exit;
    std::vector<VertexDescriptor> m_vertex_array;
    
};
//// DFA solver implementation [end]
//============================================================================================

#endif
// clang-format on
