#pragma once
#ifndef __SYNTAX_TREE_H__
#define __SYNTAX_TREE_H__
// clang-format off

//#include "syntax_visit.h"

// template <typename Graph, typename VertexProperty,
//          typename VertexDescriptor =
//            typename boost::graph_traits<Graph>::vertex_descriptor>
// class create_syntaxtree_t : public stmt_visitor_s{
//    VISITOR_DECL_BEGIN_V3
// public:
//    visit_result_t Visit_NONE(NodeType net, NodeType parent, void* user_data){
//        return Visit_NOOP(net, parent, user_data);
//    }
//    visit_result_t Visit_NOOP(NodeType net, NodeType, void*){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=2;
//        return VisitResult_Recurse;
//    }
//
//    // assign stmts
//    visit_result_t Visit_ASSIGN_NB(NodeType net, NodeType parent, void*
//    user_data){
//        return Visit_ASSIGN(net, parent, user_data);
//    }
//    visit_result_t Visit_ASSIGN(NodeType net, NodeType parent, void*
//    user_data){
//        VertexDescriptor s = *static_cast<VertexDescriptor*>(user_data);
//        VertexProperty &node_info = g[s];
//        node_info.bb = net;
//        node_info.bb_id = sequence;
//        node_info.loop_depth = loop_depth;
//
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=2;
//        return VisitResult_Recurse;
//    }
//
//    // block stmt
//    visit_result_t Visit_BLOCK(NodeType net, NodeType parent, void*
//    user_data){
//        VertexDescriptor s = *static_cast<VertexDescriptor*>(user_data);
//        //unsigned cnt = ivl_stmt_block_count(net);
//        //assert(cnt > 0); // BLOCK must contain at least one statement.
//        //bool is_exist;
//
//        //// 1. fill self
//        //VertexProperty &node_info = g[s];
//        //node_info.bb = net;
//        //node_info.bb_id = sequence;
//        //node_info.loop_depth = loop_depth;
//
//        //// 2. handle `cnt` children
//        //for (size_t i = 0; i < cnt; i++) {
//        //  // add child
//        //  auto new_child = boost::add_vertex(g);
//        //  g[new_child].sub_idx = i;
//
//        //  std::tie(std::ignore, is_exist) = boost::add_edge(s, new_child,
//        g);
//        //  assert(is_exist);
//        //  // handle current child
//        //  ivl_statement_t cur = ivl_stmt_block_stmt(net, i);
//        //  create_syntaxtree<Graph, VertexProperty>(cur, new_child, g,
//        loop_depth);
//        //}
//
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=2;
//        return VisitResult_Recurse;
//    }
//
//    // branch stmts
//    visit_result_t Visit_CASER(NodeType net, NodeType parent, void*
//    user_data){
//        return Visit_CASE(net, parent, user_data);
//    }
//    visit_result_t Visit_CASEX(NodeType net, NodeType parent, void*
//    user_data){
//        return Visit_CASE(net, parent, user_data);
//    }
//    visit_result_t Visit_CASEZ(NodeType net, NodeType parent, void*
//    user_data){
//        return Visit_CASE(net, parent, user_data);
//    }
//    visit_result_t Visit_CASE(NodeType net, NodeType parent, void* user_data){
//        VertexDescriptor s = *static_cast<VertexDescriptor*>(user_data);
//        switch(){
//            ;
//        }
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=2;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_CONDIT(NodeType net, NodeType parent, void*
//    user_data){
//        VertexDescriptor s = *static_cast<VertexDescriptor*>(user_data);
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=2;
//        return VisitResult_Recurse;
//    }
//
//    // wait stmt
//    visit_result_t Visit_WAIT(NodeType net, NodeType parent, void* user_data){
//        VertexDescriptor s = *static_cast<VertexDescriptor*>(user_data);
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=2;
//        return VisitResult_Recurse;
//    }
//
//    // loop stmt
//    visit_result_t Visit_WHILE(NodeType net, NodeType parent, void*
//    user_data){
//        VertexDescriptor s = *static_cast<VertexDescriptor*>(user_data);
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=2;
//        return VisitResult_Recurse;
//    }
//
//    //// TODO
//    visit_result_t Visit_ALLOC(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_CASSIGN(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_CONTRIB(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_DEASSIGN(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_DELAY(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_DELAYX(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_DISABLE(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_DO_WHILE(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_FORCE(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_FOREVER(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_FORK(NodeType net, NodeType parent, void* user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_FORK_JOIN_ANY(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_FORK_JOIN_NONE(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_FREE(NodeType net, NodeType parent, void* user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_RELEASE(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_REPEAT(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_STASK(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_TRIGGER(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//    visit_result_t Visit_UTASK(NodeType net, NodeType parent, void*
//    user_data){
//        m_ST_visited[(unsigned int)ivl_statement_type(net)]=1;
//        return VisitResult_Recurse;
//    }
//
// public:
//    create_syntaxtree_t(Graph& __g) : g(__g)
//    {
//        m_ST_visited.resize(31);
//        std::fill(m_ST_visited.begin(), m_ST_visited.end(), 0);
//    }
//    Graph &g;
//    std::vector<int> m_ST_visited; // IVL_ST_xxx 31. 0: no appear; 1: not
//    support; 2: success
//};

//// ast [begin]
//============================================================================================
template <typename Graph, typename VertexProperty,
          typename VertexDescriptor = typename boost::graph_traits<Graph>::vertex_descriptor>
static int create_syntaxtree(const ivl_statement_t net, VertexDescriptor s, Graph &g,
                             unsigned int loop_depth = 0) {
    static unsigned long long sequence = SEQ_START;
    const ivl_statement_type_t code = ivl_statement_type(net);
    assert(net);

    switch (code) {
    case IVL_ST_ALLOC: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_ASSIGN:
    case IVL_ST_ASSIGN_NB: {
        VertexProperty &node_info = g[s];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;
        break;
    }

    case IVL_ST_BLOCK: {
        unsigned cnt = ivl_stmt_block_count(net);
        assert(cnt > 0); // BLOCK must contain at least one statement.
        bool is_exist;

        // 1. fill self
        VertexProperty &node_info = g[s];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;

        // 2. handle `cnt` children
        for (size_t i = 0; i < cnt; i++) {
            // add child
            auto new_child = boost::add_vertex(g);
            g[new_child].sub_idx = i;

            std::tie(std::ignore, is_exist) = boost::add_edge(s, new_child, g);
            assert(is_exist);
            // handle current child
            ivl_statement_t cur = ivl_stmt_block_stmt(net, i);
            create_syntaxtree<Graph, VertexProperty>(cur, new_child, g, loop_depth);
        }
        break;
    }

    case IVL_ST_CASEX:
    case IVL_ST_CASEZ:
    case IVL_ST_CASER:
    case IVL_ST_CASE: {
        bool is_exist;

        // 1. fill self
        VertexProperty &node_info = g[s];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;

        unsigned int count = ivl_stmt_case_count(net);
        assert(count > 0);
        // unsigned int default_case = count;
        for (unsigned int idx = 0; idx < count; idx++) {
            // ivl_expr_t cex = ivl_stmt_case_expr(net, idx);
            ivl_statement_t cst = ivl_stmt_case_stmt(net, idx);

            assert(cst);
            if (cst) { // if this branch has statement. [there is no need to check
                       // null]
                auto new_child = boost::add_vertex(g);
                std::tie(std::ignore, is_exist) = boost::add_edge(s, new_child, g);
                assert(is_exist);
                g[new_child].sub_idx = idx;
                create_syntaxtree<Graph, VertexProperty>(cst, new_child, g, loop_depth);
            }
        }
        break;
    }

    case IVL_ST_CASSIGN: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_CONDIT: {
        bool is_exist;

        // 1. fill self
        VertexProperty &node_info = g[s];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;

        // 2. handle ture/false part
        ivl_expr_t ex = ivl_stmt_cond_expr(net);
        ivl_statement_t t = ivl_stmt_cond_true(net);
        ivl_statement_t f = ivl_stmt_cond_false(net);
        assert(ex);

        if (t) {
            auto new_child = boost::add_vertex(g);
            std::tie(std::ignore, is_exist) = boost::add_edge(s, new_child, g);
            assert(is_exist);
            g[new_child].is_true_part = 0; // set true part flag
            create_syntaxtree<Graph, VertexProperty>(t, new_child, g, loop_depth);
        }
        if (f) {
            auto new_child = boost::add_vertex(g);
            std::tie(std::ignore, is_exist) = boost::add_edge(s, new_child, g);
            assert(is_exist);
            g[new_child].is_true_part = 1; // set false part flag
            create_syntaxtree<Graph, VertexProperty>(f, new_child, g, loop_depth);
        }
        break;
    }

    case IVL_ST_CONTRIB: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_DEASSIGN: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    // case IVL_ST_DELAY:
    // case IVL_ST_DELAYX:{
    //    bool is_exist;

    //    // 1. fill wait node info
    //    VertexProperty &node_info = g[s];
    //    node_info.bb = net;
    //    node_info.bb_id = sequence;
    //    node_info.loop_depth = loop_depth;

    //    // 2. handle sub stmt
    //    auto new_child = boost::add_vertex(g);
    //    std::tie(std::ignore, is_exist) = boost::add_edge(s, new_child, g);
    //    assert(is_exist);
    //    ivl_statement_t cur = ivl_stmt_sub_stmt(net);
    //    create_syntaxtree<Graph, VertexProperty>(cur, new_child, g, loop_depth);

    //    // show_stmt_delayx(net, ind);
    //    break;
    //}

    case IVL_ST_DISABLE: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // show_stmt_disable(net, ind);
        break;
    }

    case IVL_ST_DO_WHILE: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_FORCE: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        // show_stmt_force(net, ind);
        break;
    }

    case IVL_ST_FORK: {
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
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_NOOP: {
        // node->set_type(CFG_NODE_NORMAL);
        // node->set_stmt(net);
        break;
    }

    case IVL_ST_RELEASE: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_STASK: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_TRIGGER: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_UTASK: {
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
        VertexProperty &node_info = g[s];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;

        // 2. handle sub stmt
        auto new_child = boost::add_vertex(g);
        std::tie(std::ignore, is_exist) = boost::add_edge(s, new_child, g);
        assert(is_exist);
        ivl_statement_t cur = ivl_stmt_sub_stmt(net);
        create_syntaxtree<Graph, VertexProperty>(cur, new_child, g, loop_depth);

        break;
    }

    case IVL_ST_REPEAT:
    case IVL_ST_WHILE: {
        loop_depth++; // increase the depth of loop nesting
        bool is_exist;

        // 1. fill self
        VertexProperty &node_info = g[s];
        node_info.bb = net;
        node_info.bb_id = sequence;
        node_info.loop_depth = loop_depth;

        // 2. handle loop body
        auto new_child = boost::add_vertex(g);
        std::tie(std::ignore, is_exist) = boost::add_edge(s, new_child, g);
        assert(is_exist);
        ivl_statement_t cur = ivl_stmt_sub_stmt(net);
        assert(cur);
        create_syntaxtree<Graph, VertexProperty>(cur, new_child, g, loop_depth);
        break;
    }

    case IVL_ST_FOREVER: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    case IVL_ST_NONE: {
        __DBegin(> 0);
        DFA_NOT_SUPPORT(net);
        __DEnd;
        break;
    }

    default: { NetAssertWrongPath(net); }
    }

    sequence++;
    return 0;
}

template <typename BasicBlock> struct tree_node_s {
    BasicBlock bb{nullptr}; // bb of entry/exit nodes are nullptr.
    unsigned int bb_id{0};  // sequence
    unsigned int loop_depth{0};
    unsigned int is_true_part{
        0};                  // for if-stmt. indicate current stmt is true part(1)/false part(2)
    unsigned int sub_idx{0}; // brother index. for non-block(begin-end/fork-join), value is 0
};

template <typename BasicBlock /*, typename T, typename TCompare = std::less<T>*/>
class syntaxtree_t {
  public:
    //// typedef [begin]
    using node_property_t = tree_node_s<BasicBlock>;
    using graph_t =
        boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, node_property_t,
                              boost::no_property, boost::no_property, boost::listS>;
    using VertexDescriptor = typename boost::graph_traits<graph_t>::vertex_descriptor;
    using EdgeDescriptor = typename boost::graph_traits<graph_t>::edge_descriptor;
    using OutEdgeIter = typename boost::graph_traits<graph_t>::out_edge_iterator;
    using VertexIter = typename boost::graph_traits<graph_t>::vertex_iterator;
    using InEdgeIter = typename boost::graph_traits<graph_t>::in_edge_iterator;
    //// typedef [end]

    syntaxtree_t(const BasicBlock &net) {
        int re = create_tree(net);
        assert(re == 0);
    }

    // template<typename AstNode>
    int create_tree(const BasicBlock &net) {
        clear_tree();

        bool is_exist;
        m_root = boost::add_vertex(m_tree);
        m_tree[m_root].bb_id = 1; // bb_id==1 => this node is root-node

        auto new_child = boost::add_vertex(m_tree);
        std::tie(std::ignore, is_exist) = boost::add_edge(m_root, new_child, m_tree);
        assert(is_exist);

        //int re = create_syntaxtree<graph_t, node_property_t>(net, m_root, m_tree, 0);
        int re = create_syntaxtree<graph_t, node_property_t>(net, new_child, m_tree, 0);
        assert(re == 0);

        return 0;
    }

    // const VertexDescriptor& get_vertex_by_bb(const BasicBlock& net){
    //  assert(boost::in_degree(s, m_tree)==1);
    //  InEdgeIter e_it;
    //  std::tie(e_it, std::ignore) = boost::in_edges(s, m_tree);
    //  return boost::source(*e_it, m_tree);
    //}

    const node_property_t &get_parent_property(const VertexDescriptor &s) {
        assert(boost::in_degree(s, m_tree) == 1);
        InEdgeIter e_it;
        std::tie(e_it, std::ignore) = boost::in_edges(s, m_tree);
        auto father = boost::source(*e_it, m_tree);
        return m_tree[father];
    }

    void clear_tree() {
        m_tree.clear();
        m_root = 0;
    }

    // do custom print
    template <typename VertexPrinter> void dump_graph(const char *name, VertexPrinter &&v_printer) {
        FILE *p_file = fopen((std::string(name) + ".dot").c_str(), "w");
        dump_digraph(name, m_tree, v_printer);
        fclose(p_file);
    }

    graph_t clone_tree() { return m_tree; }

    graph_t &get_tree() { return m_tree; }

    VertexDescriptor get_root() { return m_root; }

  private:
    graph_t m_tree;
    VertexDescriptor m_root;
};

template <typename Graph, typename Vertex = typename boost::graph_traits<Graph>::vertex_descriptor>
inline void stmttree_v_printer(FILE *p_file, const Vertex &s, const Graph &g) {
    const auto &vp = g[s];
    ivl_statement_t bb = vp.bb;
    // unsigned int bb_id = vp.bb_id;
    // unsigned int loop_depth = vp.loop_depth;
    unsigned int is_true_part = vp.is_true_part;
    unsigned int sub_idx = vp.sub_idx;

    std::string label_content = "sub_idx:" + std::to_string(sub_idx) + ", is_true_part:" +
                                std::to_string(is_true_part) + ", stmttype:" +
                                std::to_string(bb == nullptr ? -1 : (int)ivl_statement_type(bb));

    fprintf(p_file, "V%lld [label=\"lineno=%d\n%s\"];\n", (long long)s,
            bb == nullptr ? -1 : (int)ivl_stmt_lineno(bb), label_content.c_str());
};

//// ast [end]
//============================================================================================

// clang-format on
#endif
