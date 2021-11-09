#pragma once
#ifndef __DOM_TREE_H__
#define __DOM_TREE_H__
// clang-format off

//#include <boost/core/lightweight_test.hpp>
//#include <iostream>
//#include <algorithm>
#include "graph_helper.h"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dominator_tree.hpp>
#include <algorithm>

/*
    predmap: dom_vertex->dom_vertex
    tree: 
*/
/* 
    create a tree from predecessor mapping
    algorithm:
        tmp_map; // mapping tree_property -> tree_vertex
        for(each [key, val] in pred_map)
        {
            // here 'key' is child, 'val' is parent of 'key'
            
        }
*/
template<
    typename Graph, 
    typename PredStdMap, 
    typename __TreeVertex = typename boost::graph_traits<Graph>::vertex_descriptor,
    typename __Key = typename PredStdMap::key_type>
void create_tree_from_predmap(
    Graph& tree,
    std::unordered_map<__TreeVertex, __Key>& vertex_key_map,
    std::unordered_map<__Key, __TreeVertex>& key_vertex_map,
    const PredStdMap& predmap,
    typename PredStdMap::key_type root /*root vertex of predmap */)
{
    assert(boost::num_vertices(tree)==0);
    assert(boost::num_edges(tree)==0);

    using TreeVertex = typename boost::graph_traits<Graph>::vertex_descriptor;
    using Key = typename PredStdMap::key_type;
    using __Value = typename PredStdMap::mapped_type;
    
    static_assert(std::is_same<TreeVertex, __TreeVertex>::value, "");
    static_assert(std::is_same<Key, __Key>::value, "");
    static_assert(std::is_same<Key, __Value>::value, "");
    
    //  predmap vertex(key) -> tree_vertex
    //std::unordered_map<Key, TreeVertex> key_vertex_map;
    
    bool is_exist;
    typename PredStdMap::iterator search;
    for(const auto& k_v: predmap){
        const auto& u_prop = k_v.second;
        const auto& v_prop = k_v.first;
        if(v_prop != root){
            TreeVertex u;
            TreeVertex v;
            
            search = key_vertex_map.find(u_prop);
            if(search != key_vertex_map.end()){ // found
                u = search->second;
            }
            else{
                //if not found, then create new vertex
                u = boost::add_vertex(tree);
                key_vertex_map[u_prop] = u;
                vertex_key_map[u] = u_prop;
            }

            search = key_vertex_map.find(v_prop);
            if(search != key_vertex_map.end()){ // found
                v = search->second;
            }
            else{
                v = boost::add_vertex(tree);
                key_vertex_map[v_prop] = v;
                vertex_key_map[v] = v_prop;
            }
            
            // connect u->v
            std::tie(std::ignore, is_exist) = boost::add_edge(u, v, tree);
            assert(is_exist);
        }
        else{
            // if root has not been insert this tree, 
            // then create a new vertex and save the root property(here is 'v_prop')
            search = key_vertex_map.find(v_prop);
            if(search == key_vertex_map.end()){ // not found
                TreeVertex v = boost::add_vertex(tree);
                key_vertex_map[v_prop] = v;
                vertex_key_map[v] = v_prop;
            }
        }
    }
}

template<
    typename Graph, 
    typename Vertex>
void __tree_depth(
    std::unordered_map<Vertex, size_t>& depth,
    size_t cur_depth,
    const Graph& tree,
    const Vertex& cur)
{
    using OutEdgeIter = typename boost::graph_traits<Graph>::out_edge_iterator;
    
    depth[cur] = cur_depth;
    OutEdgeIter e_it, e_end;
    std::tie(e_it, e_end) = boost::out_edges(cur, tree);
    for (; e_it != e_end; e_it++) {
        Vertex child = boost::target(*e_it, tree);
        __tree_depth<Graph, Vertex>(depth, cur_depth+1, tree, child);
    }
}

// compute every node's depth of a tree(stored in a predecessor map)
template<
    typename Graph, 
    typename Vertex=typename boost::graph_traits<Graph>::vertex_descriptor>
void compute_tree_depth(
    std::unordered_map<Vertex, size_t>& depth,
    const Graph& tree,
    const Vertex& root)
{
    static_assert(
        std::is_same<Vertex, 
            typename boost::graph_traits<Graph>::vertex_descriptor>::value, "");
    
    __tree_depth<Graph, Vertex>(depth, 0, tree, root);
}

/*
    Graph1: input cfg
    Graph2: cfg for compute dominate tree
    Graph3: dom tree
*/
template <typename Graph1, bool IsPostDom = false> class domtree_t {
  public:
    // Graph1: input CFG. passed from outer CFG
    using Vertex1 = typename boost::graph_traits<Graph1>::vertex_descriptor;
    
    // Graph2: CFG for compute dom-tree. convert from Graph1
    using Graph2 = boost::adjacency_list<
        boost::listS, 
        boost::listS, 
        boost::bidirectionalS,
        boost::property<boost::vertex_index_t, std::size_t>, 
        boost::no_property,
        boost::no_property,
        boost::listS>;
    using Vertex2 = typename boost::graph_traits<Graph2>::vertex_descriptor;
    using Edge2 = typename boost::graph_traits<Graph2>::edge_descriptor;
    using OutEdgeIter2 = typename boost::graph_traits<Graph2>::out_edge_iterator;
    using InEdgeIter2 = typename boost::graph_traits<Graph2>::in_edge_iterator;
    using VertexIter2 = typename boost::graph_traits<Graph2>::vertex_iterator;
    
    // Graph3: dom tree
    using Graph3 = boost::adjacency_list<
        boost::listS, 
        boost::listS, 
        boost::bidirectionalS,
        Vertex2, 
        boost::no_property,
        boost::no_property,
        boost::listS>;
    using Vertex3 = typename boost::graph_traits<Graph3>::vertex_descriptor;
    using Edge3 = typename boost::graph_traits<Graph3>::edge_descriptor;
    using OutEdgeIter3 = typename boost::graph_traits<Graph3>::out_edge_iterator;
    using InEdgeIter3 = typename boost::graph_traits<Graph3>::in_edge_iterator;
    using VertexIter3 = typename boost::graph_traits<Graph3>::vertex_iterator;

    //using IndexMap = 
    //    typename boost::property_map<Graph2, boost::vertex_index_t>::type;
    //using PredMap =
    //    boost::iterator_property_map<
    //        typename std::vector<Vertex2>::iterator, 
    //        IndexMap>;

    using V1V2Map_t = std::unordered_map<Vertex1, Vertex2>;
    using V2V1Map_t = std::unordered_map<Vertex2, Vertex1>;
    using V2PredMap_t = std::unordered_map<Vertex2, Vertex2>;

    domtree_t() { /* empty domtree */
    }

    // entry is for pre-dom tree
    // exit is for post-dom tree
    domtree_t(const Graph1 &g, // old graph
              Vertex1 entry, Vertex1 exit = boost::graph_traits<Graph1>::null_vertex()) {
        make_domtree_from_cfg<IsPostDom>(g, entry, exit);
    }

    template <bool is_post>
    void make_domtree_from_cfg(const Graph1 &g, Vertex1 entry, Vertex1 exit) {
        V2V1Map_t v2v1_map;
        V1V2Map_t v1v2_map;
        
        using IndexMap = 
            typename boost::property_map<
                Graph2, boost::vertex_index_t>::type;
        using PredMap =
            boost::iterator_property_map<
                typename std::vector<Vertex2>::iterator, 
                IndexMap>;
        
        //// 1. G1 => {G2, V2->V1, V1->V2}  //mean: inputs => outputs
        if (is_post == false) {
            auto vertex_copy = [&v2v1_map, &v1v2_map](Vertex1 old_v, Vertex2 new_v) -> void {
                v2v1_map[new_v] = old_v;
                v1v2_map[old_v] = new_v;
            };
            graph_relation_copy(g, m_cfg_dom, vertex_copy);
            m_entry = v1v2_map[entry];
        } else {
            auto vertex_copy = [&v2v1_map, &v1v2_map](Vertex1 old_v, Vertex2 new_v) -> void {
                v2v1_map[new_v] = old_v;
                v1v2_map[old_v] = new_v;
            };
            using Edge1 = typename boost::graph_traits<Graph1>::edge_descriptor;
            using Edge2 = typename boost::graph_traits<Graph2>::edge_descriptor;
            auto edge_copy = [this](Edge1, Edge2 new_e) -> void {
                Vertex2 u = boost::source(new_e, m_cfg_dom);
                Vertex2 v = boost::target(new_e, m_cfg_dom);
                boost::remove_edge(new_e, m_cfg_dom);
                boost::add_edge(v, u, m_cfg_dom);
            };
            graph_relation_copy(g, m_cfg_dom, vertex_copy, edge_copy);

            __DBegin(== 2);
            {
                auto vertex_printer = [&g, &v2v1_map](FILE *p_file, Vertex2 s,
                                                      const Graph2 &) -> void {
                    // ivl_statement_t stmt = g[m_v3v1_map[m_v2v3_map[s]]].bb;
                    ivl_statement_t stmt = g[v2v1_map[s]].bb;
                    int lineno = stmt ? (int)ivl_stmt_lineno(stmt) : -1;
                    fprintf(p_file, "V%lld [label=\"line:%d\"];\n", (long long)s, lineno);
                };

                dump_digraph("cfg_transpose", m_cfg_dom, vertex_printer);
            }
            __DEnd;
            m_entry = v1v2_map[exit];

            //// find entry
            // graph_vertex_visit(m_cfg_dom, [this](Vertex2 s, Graph2& g) -> void
            //{
            //    ivl_statement_t stmt = g[v2v1_map[s]].bb;
            //    int lineno = stmt ? (int)ivl_stmt_lineno(stmt) : -1;
            //    fprintf(p_file, "V%lld [label=\"line:%d\"];\n", (long long)s, lineno);
            //});
        }

        //// 2. G2 => PredStdMap(V2->V2)

        IndexMap indexMap(boost::get(boost::vertex_index, m_cfg_dom));
        VertexIter2 uItr, uEnd;
        unsigned int i_offset = 0;
        for(std::tie(uItr, uEnd) = boost::vertices(m_cfg_dom);
            uItr != uEnd; 
            ++uItr, ++i_offset)
        {
            boost::put(indexMap, *uItr, i_offset);
        }
        m_v2_predvector = 
            std::vector<Vertex2>(
                boost::num_vertices(m_cfg_dom), 
                boost::graph_traits<Graph2>::null_vertex()
            ); // alloc storage

        // Lengauer-Tarjan dominator tree algorithm
        PredMap domTreePredMap
            = boost::make_iterator_property_map(m_v2_predvector.begin(), indexMap);

        boost::lengauer_tarjan_dominator_tree(m_cfg_dom, m_entry, domTreePredMap);
        
        VertexIter2 v_it, v_end;
        for(std::tie(v_it, v_end) = boost::vertices(m_cfg_dom);
            v_it != v_end; ++v_it)
        {
            // convert PredMap to std::unordered_map
            Vertex2 key = *v_it;
            Vertex2 val = boost::get(domTreePredMap, key);
            m_v2_predmap.insert({key, val});
        }
        
        //// 3. PredStdMap => {G3, V3->V2}
        create_tree_from_predmap<Graph3, V2PredMap_t>(
            m_domTree,
            m_v3v2_map,
            m_v2v3_map,
            m_v2_predmap,
            m_entry);
        
        //// 4. G3 => V3->depth
        compute_tree_depth<Graph3, Vertex3>(
            m_v3_depth,
            m_domTree,
            m_v2v3_map[m_entry]);

        //// 5. {V3->V2, V2->V1} => V3->V1
        for(auto k_v: m_v3v2_map){
            const auto& v3 = k_v.first;
            const auto& v2 = k_v.second;
            const auto& v1 = v2v1_map[v2];
            m_v3v1_map.insert({v3, v1});
            m_v1v3_map.insert({v1, v3});
        }
    }

    Vertex1 get_idom(Vertex1 v1){
        Vertex3 v3 = m_v1v3_map[v1];
        return get_idom(v3);
    }

    // compute lowest common ancestor
    template <typename InputIt1 /*V1*/> Vertex1 plain_lca(InputIt1 first, InputIt1 last) {
        assert(std::distance(first, last) > 0);
        assert(first != last);

        std::vector<Vertex3> v3_vec;
        for (; first != last; first++) {
            v3_vec.emplace_back(m_v1v3_map[*first]);
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

    Vertex1 plain_lca(const std::vector<Vertex1> &v1_vec) {
        return plain_lca(v1_vec.begin(), v1_vec.end());
    }

    // return true, if "a dom b" satisfied. (a is dominator)
    bool is_dom(Vertex1 a, Vertex1 b) {
        auto ancestor = plain_lca({a, b});
        return (ancestor == a);
    }

    void dump_graph(const char* name, const Graph1& cfg){
        auto vertex_printer = [&cfg, this](
            FILE* p_file, Vertex3 s, const Graph3& ) -> void
        {
            ivl_statement_t stmt = cfg[m_v3v1_map[s]].bb; // cfg must have bb property
            int lineno = stmt ? (int)ivl_stmt_lineno(stmt) : -1;
            fprintf(p_file, "V%lld [label=\"line:%d\"];\n", (long long)s, lineno);
        };
        dump_digraph(name, m_domTree, vertex_printer);
    }

private:
    Vertex3 get_idom(Vertex3 v3)
    {
        assert(1 == boost::in_degree(v3, m_domTree));
        InEdgeIter3 e_it;
        std::tie(e_it, std::ignore) = boost::in_edges(v3, m_domTree);
        return boost::source(*e_it, m_domTree);
    }

    Graph2 m_cfg_dom; // cfg for dom-tree
    Vertex2 m_entry;
    std::unordered_map<Vertex2, Vertex2> m_v2_predmap;
    std::vector<Vertex2> m_v2_predvector;
    
    Graph3 m_domTree;
    std::unordered_map<Vertex3, Vertex2> m_v3v2_map;
    std::unordered_map<Vertex2, Vertex3> m_v2v3_map;
    std::unordered_map<Vertex2, size_t> m_v3_depth; // every depth of vertex in Graph3
    
    std::unordered_map<Vertex3, Vertex1> m_v3v1_map;
    std::unordered_map<Vertex1, Vertex3> m_v1v3_map;
};

// clang-format on
#endif
