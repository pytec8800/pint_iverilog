#pragma once
#ifndef __GRAPH_HELPER_H__
#define __GRAPH_HELPER_H__
// clang-format off

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/copy.hpp>
//#include <boost/graph/transpose_graph.hpp>
#include <tuple>
#include <type_traits>
#include <iostream>

/*
    copy graph
type:
    VertexCopier: funtor, prototype is
        "void fn(VertexI v_in, VertexO v_out);"
    EdgeCopier: funtor, prototype is
        "void fn(edge_descriptor e_in, edge_descriptor e_out)"
*/
template<
    typename Graph, 
    typename MutableGraph, 
    typename VertexCopier,
    typename EdgeCopier
>
inline void graph_relation_copy(
    const Graph& g_in, 
    MutableGraph& g_out, 
    VertexCopier&& vertex_copy,
    EdgeCopier&& edge_copy)
{
    using VertexO = typename boost::graph_traits<MutableGraph>::vertex_descriptor;
    using VertexI = typename boost::graph_traits<Graph>::vertex_descriptor;
    using EdgeO = typename boost::graph_traits<MutableGraph>::edge_descriptor;
    using EdgeI = typename boost::graph_traits<Graph>::edge_descriptor;
    //using OutEdgeIterO = typename boost::graph_traits<MutableGraph>::out_edge_iterator;
    //using OutEdgeIterI = typename boost::graph_traits<Graph>::out_edge_iterator;
    //using VertexIterO = typename boost::graph_traits<MutableGraph>::vertex_iterator;
    using VertexIterI = typename boost::graph_traits<Graph>::vertex_iterator;
    using EdgeIterI = typename boost::graph_traits<Graph>::edge_iterator;
    //using IOPair = std::pair<VertexI, VertexO>;
    using cvt = boost::detail::remove_reverse_edge_descriptor<Graph, EdgeI>;

    std::unordered_map<VertexI, VertexO> i_o_map(boost::num_vertices(g_in));

    VertexIterI vi, vi_end;
    for (std::tie(vi, vi_end) = boost::vertices(g_in); vi!=vi_end; ++vi)
    {
        VertexO new_v = add_vertex(g_out);
        i_o_map[*vi] = new_v;
        //if( !Ignore(VertexCopier) ){
            vertex_copy(*vi, new_v);
        //}
    }
    EdgeIterI ei, ei_end;
    for (std::tie(ei, ei_end) = boost::edges(g_in); ei != ei_end; ++ei)
    {
        EdgeO new_e;
        bool inserted;
        std::tie(new_e, inserted)
            = add_edge(
                i_o_map[boost::source(*ei, g_in)],
                i_o_map[boost::target(*ei, g_in)],
                g_out);
        assert(inserted);
        //if( !Ignore(EdgeCopier) ){
            edge_copy(cvt::convert(*ei, g_in), new_e);
        //}
    }
}

template<
    typename Graph, 
    typename MutableGraph,
    typename VertexCopier>
inline void graph_relation_copy(
    const Graph& g_in, 
    MutableGraph& g_out,
    VertexCopier&& vertex_copy)
{
    //using VertexO = typename boost::graph_traits<MutableGraph>::vertex_descriptor;
    //using VertexI = typename boost::graph_traits<Graph>::vertex_descriptor;
    //auto null_vertex_copy = [](VertexI, VertexO) -> void {};
    
    using EdgeO = typename boost::graph_traits<MutableGraph>::edge_descriptor;
    using EdgeI = typename boost::graph_traits<Graph>::edge_descriptor;
    auto null_edge_copy = [](EdgeI, EdgeO) -> void {};
    
    graph_relation_copy(
        g_in, 
        g_out, 
        vertex_copy,
        null_edge_copy);
}

// dump directed graph to a dot file
/*
    VertexPrinter: void callback(FILE*, VertexDescriptor, const Graph&);
*/
template<typename Graph, typename VertexPrinter/*, typename EdgePrinter*/>
inline void dump_digraph(const char *name, const Graph& graph, VertexPrinter&& v_printer)
{
    using VertexIter = typename boost::graph_traits<Graph>::vertex_iterator;
    using VertexDescriptor = typename boost::graph_traits<Graph>::vertex_descriptor;
    using OutEdgeIter = typename boost::graph_traits<Graph>::out_edge_iterator;

    FILE *p_file = fopen((std::string(name) + ".dot").c_str(), "w");

    auto vis_dump = [&p_file, &v_printer](
        const VertexDescriptor &s, const Graph &g) -> int 
    {
        // print vertex
        v_printer(p_file, s, g);

        // print out edge
        OutEdgeIter e_it, e_end;
        for (std::tie(e_it, e_end) = boost::out_edges(s, g); e_it != e_end; e_it++) {
            auto v = boost::target(*e_it, g);
            fprintf(p_file, "V%lld->V%lld;\n", (long long)s, (long long)v);
        }
        return 0;
    };

    VertexIter v_it, v_end;
    fprintf(p_file, "digraph %s {\n", name);
    //visit_vertices(vis_dump);
    for(std::tie(v_it, v_end)=boost::vertices(graph);
        v_it!=v_end;
        v_it++)
    {
        int re = vis_dump(*v_it, graph);
        assert(re == 0);
    }
    fprintf(p_file, "}\n");
    fclose(p_file);
}

template <typename Graph, typename Visitor>
inline void graph_vertex_visit(Graph &g, Visitor &&vis) {
    using VertexIter = typename boost::graph_traits<Graph>::vertex_iterator;

    VertexIter vi, vi_end;
    for (std::tie(vi, vi_end) = boost::vertices(g); vi != vi_end; ++vi) {
        vis(*vi, g);
    }
}

// clang-format on
#endif
