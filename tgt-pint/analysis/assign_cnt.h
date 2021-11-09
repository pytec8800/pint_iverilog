#pragma once
#ifndef __ASSIGN_CNT_H__
#define __ASSIGN_CNT_H__
// clang-format off
#include "priv.h"

//#define IncAcnt(x) ( (((x)+1)>2) ? 2 : ((x)+1) )
#define Saturat2(x) ( ((x)>2) ? 2 : (x) )

// 'acnt' = assign cnt
struct sig_acnt_s;
using signal_acnt_list_t = std::set<sig_acnt_s>;
using assign_cnt_solver_t = DFA_solver_t<ivl_statement_t, sig_acnt_s>;

struct sig_acnt_s {
    ivl_signal_t m_sig={nullptr}; // key
    std::vector<int8_t> m_acnts;  // 'acnt' = assign cnt

    sig_acnt_s(const ivl_signal_t &sig) : m_sig(sig) {
        size_t width;
        size_t arr_word = ivl_signal_array_count(sig);
        if(arr_word==1)
            width = ivl_signal_width(sig);
        else
            width = arr_word;
        
        m_acnts.resize(width);
        std::fill(m_acnts.begin(), m_acnts.end(), 0);
    }
    //sig_acnt_s(const ivl_signal_t &sig, size_t width) : m_sig(sig) {
    //    m_acnts.resize(width);
    //    std::fill(m_acnts.begin(), m_acnts.end(), 0);
    //}
    //sig_acnt_s(const ivl_signal_t &sig, const std::vector<int8_t>& acnts)
    //    : m_sig(sig), m_acnts(acnts) {}

    bool operator<(const sig_acnt_s &r) const { return this->m_sig < r.m_sig; }
    bool operator==(const sig_acnt_s &r) const {
        if (this->m_sig == r.m_sig) {
            return (this->m_acnts == r.m_acnts);
        } else {
            return false;
        }
    }
};

// It's usually used for union. (C = A U B)
void append_acnt(signal_acnt_list_t &list, const sig_acnt_s &e) {
    auto sig = e.m_sig;
    auto search = list.find(sig);
    if (search == list.end()) { // not found.
        list.emplace(e);        // insert immediately
    } else {                    // found
        assert(e.m_sig == search->m_sig);
        // merge acnt
        sig_acnt_s tt = *search;
        assert(tt.m_acnts.size() == e.m_acnts.size());
        for(size_t i=0; i<tt.m_acnts.size(); i++){
            tt.m_acnts[i] = Saturat2(tt.m_acnts[i] + e.m_acnts[i]);
        }
        list.erase(search);
        list.emplace(std::move(tt));
    }
}

// use Max for merge
void append_acnt_max(signal_acnt_list_t &list, const sig_acnt_s &e) {
    auto sig = e.m_sig;
    auto search = list.find(sig);
    if (search == list.end()) { // not found.
        list.emplace(e);        // insert immediately
    } else {                    // found
        assert(e.m_sig == search->m_sig);
        // merge acnt
        sig_acnt_s tt = *search;
        assert(tt.m_acnts.size() == e.m_acnts.size());
        for(size_t i=0; i<tt.m_acnts.size(); i++){
            //tt.m_acnts[i] = Saturat2(tt.m_acnts[i] + e.m_acnts[i]);
            tt.m_acnts[i] = std::max(tt.m_acnts[i], e.m_acnts[i]);
            assert(tt.m_acnts[i]<=2);
        }
        list.erase(search);
        list.emplace(std::move(tt));
    }
}

inline int null_acnt_gen(signal_acnt_list_t &, const ivl_statement_t &) { return 0; }
inline int null_acnt_kill(signal_acnt_list_t &, const ivl_statement_t &) { return 0; }
inline int null_acnt_meet(signal_acnt_list_t &, const signal_acnt_list_t &, const signal_acnt_list_t &) {return 0;}
inline int null_acnt_transfer(signal_acnt_list_t &, const signal_acnt_list_t &, const signal_acnt_list_t &, const signal_acnt_list_t &) {return 0;}

void print_flow_value(const signal_acnt_list_t& fv_set, std::string& label_content)
{
    for (const auto &e: fv_set)
    {
        auto sig = e.m_sig;
        const char *sig_name = ivl_signal_basename(sig);

        label_content.append(sig_name);
        label_content.append("[");

        for (const auto &num : e.m_acnts)
        {
            label_content.append(std::to_string(num));
            label_content.append(",");
        }

        label_content.append("],\n");
    }
};

inline int acnt_gen(signal_acnt_list_t &gen_out, const ivl_statement_t &net) 
{
    assert(net);
    assert(gen_out.empty());

    auto code = ivl_statement_type(net);
    if (code == IVL_ST_ASSIGN/* || code == IVL_ST_ASSIGN_NB*/) {
        unsigned int n_lval = ivl_stmt_lvals(net);
        for (unsigned int i = 0; i < n_lval; i++) {
            ivl_lval_t lval = ivl_stmt_lval(net, i);
            ivl_signal_t sig = ivl_lval_sig(lval);
            
            if (ivl_signal_array_count(sig) == 1) // not array
            {
                sig_acnt_s e(sig);
                ivl_expr_t part_off = ivl_lval_part_off(lval);
                if(part_off == NULL) // not part
                {
                    // all bit inc
                    for(auto& i: e.m_acnts) i = 1;
                }
                else
                { 
                    // part select
                    unsigned int width = ivl_lval_width(lval);
                    
                    if(ivl_expr_type(part_off) == IVL_EX_NUMBER)
                    {
                        //TODO
                        unsigned int base_idx = 
                            (unsigned int)pint_get_ex_number_value_lu(part_off, false);
                        
                        // fprintf(stderr, "sig:%s, base:%u, width:%u\n",
                        //     ivl_signal_basename(sig), base_idx, width);
                        
                        for(size_t i=base_idx; i<base_idx+width; i++)
                        {
                            e.m_acnts[i]=1;
                        }
                    }
                    else{
                        // all bit set 1
                        assert(width<=e.m_acnts.size());
                        for(auto& i: e.m_acnts) i = 1;
                    }
                }
                gen_out.emplace(std::move(e));
            }
            else{
                //ivl_expr_t idx = ivl_lval_idx(lval);
                //if(ivl_expr_type(idx) == IVL_EX_NUMBER)
                //{
                //    unsigned int base_idx = 
                //        (unsigned int)pint_get_ex_number_value_lu(idx, false);
                //    
                //    unsigned int width = ivl_signal_width(sig);
                //    fprintf(stderr, "sig:%s, array_idx:%u, width:%u\n",
                //        ivl_signal_basename(sig), base_idx, width);
                //    
                //    e.m_acnts[base_idx] = 1; // element
                //}
            }
        }
    }
    return 0;
}

inline int acnt_transfer(
    signal_acnt_list_t &out, 
    const signal_acnt_list_t &in,
    const signal_acnt_list_t &gen, 
    const signal_acnt_list_t &kill)
{
    //__DBegin(==10);
    //{
    //    std::string label_content;
    //    
    //    label_content.append("in:\n");
    //    print_flow_value(in, label_content);
    //    label_content.append("gen:\n");
    //    print_flow_value(gen, label_content);

    //    fprintf(stderr, "\nacnt transfer:\n%s\n", label_content.c_str());
    //}
    //__DEnd;
    
    // out = gen U (in-kill)
    assert(kill.empty());
    out = in;
    for (const auto &e : gen) {
        append_acnt(out, e);
    }
    
    //__DBegin(==10);
    //{
    //    std::string label_content;
    //    
    //    label_content.append("out:\n");
    //    print_flow_value(out, label_content);

    //    fprintf(stderr, "\nacnt transfer:\n%s\n", label_content.c_str());
    //}
    //__DEnd;

    return 0;
}

inline int acnt_meet(
    signal_acnt_list_t &in, 
    const signal_acnt_list_t &prev_in1,
    const signal_acnt_list_t &prev_in2)
{
    //__DBegin(==10);
    //{
    //    std::string label_content;
    //    
    //    label_content.append("prev_in1:\n");
    //    print_flow_value(prev_in1, label_content);
    //    label_content.append("prev_in2:\n");
    //    print_flow_value(prev_in2, label_content);

    //    fprintf(stderr, "\nacnt meet:\n%s\n", label_content.c_str());
    //}
    //__DEnd;
    signal_acnt_list_t tmp = prev_in1; // WARNING: using tmp because in/in1/in2 could be the same.
    for (const auto &e : prev_in2) {
        assert( !e.m_acnts.empty() );
        //append_acnt(tmp, e);
        append_acnt_max(tmp, e);
    }
    in = tmp;
    
    //__DBegin(==10);
    //{
    //    std::string label_content;
    //    
    //    label_content.append("after in:\n");
    //    print_flow_value(in, label_content);

    //    fprintf(stderr, "\nacnt meet:\n%s\n", label_content.c_str());
    //}
    //__DEnd;
    return 0;
}

template<typename ArrDontGen, typename ArrLoopHeadr>
struct acnt_gen_s
{
    acnt_gen_s(const ArrDontGen& a1, const ArrLoopHeadr& a2, std::set<ivl_signal_t>& a3):
        m_dontgen(a1), m_loopheadr(a2), m_arr_and_part(a3) {}
    
    int operator()(signal_acnt_list_t &gen_out, const ivl_statement_t &net)
    {
        assert(net);
        assert(gen_out.empty());
        
        //// let generate at loop-header's('i=0') output
        auto search1 = m_loopheadr.equal_range(net);
        for(auto it=search1.first; it!=search1.second; it++)
        {
            sig_acnt_s e(it->second);
            for(auto& i: e.m_acnts) i = 1;
            gen_out.emplace(std::move(e));
        }
        
        auto code = ivl_statement_type(net);
        if (code == IVL_ST_ASSIGN/* || code == IVL_ST_ASSIGN_NB*/)
        {
            unsigned int n_lval = ivl_stmt_lvals(net);
            for (unsigned int i = 0; i < n_lval; i++)
            {
                ivl_lval_t lval = ivl_stmt_lval(net, i);
                ivl_signal_t sig = ivl_lval_sig(lval);
                
                // ignore dontgen
                auto search2 = m_dontgen.find({net, lval});
                if(search2 == m_dontgen.end()) // not found
                {
                    if (ivl_signal_array_count(sig) > 1) // array
                    {
                        sig_acnt_s e(sig);
                        ivl_expr_t part_off = ivl_lval_part_off(lval);
                        if(part_off == NULL) // not part
                        {
                            ivl_expr_t idx = ivl_lval_idx(lval);
                            if(ivl_expr_type(idx) == IVL_EX_NUMBER)
                            {
                                unsigned int base_idx = 
                                    (unsigned int)pint_get_ex_number_value_lu(idx, false);
                                //unsigned int arr_words = ivl_signal_array_count(sig);
                                //fprintf(stderr, "sig:%s, array_idx:%u, arr_words:%u\n",
                                //    ivl_signal_basename(sig), base_idx, arr_words);
                                e.m_acnts[base_idx] = 1; // element
                            }
                            else{
                                // all bit set 1
                                //assert(width<=e.m_acnts.size());
                                for(auto& i: e.m_acnts) i = 1;
                            }
                        }
                        else
                        {
                            m_arr_and_part.emplace(sig);
                        }
                        gen_out.emplace(std::move(e));
                    }
                }
                else{
                    // ignore dontgen
                }
            }
        }
        
        //__DBegin(== 10);
        //{
        //    if(net)
        //    {
        //        fprintf(stderr, "\nstmtlineno:%u\n", ivl_stmt_lineno(net));
        //        std::string label_content;
        //        label_content.append("gen:\n");
        //        print_flow_value(gen_out, label_content);
        //    }
        //}
        //__DEnd;
        return 0;
    }

    const ArrDontGen& m_dontgen;
    const ArrLoopHeadr& m_loopheadr;
    std::set<ivl_signal_t>& m_arr_and_part;
};

template<
    typename Graph, 
    typename Vertex=
        typename boost::graph_traits<Graph>::vertex_descriptor>
void assigncnt_v_printer(FILE* p_file, const Vertex &s, const Graph &g)
{
    const auto& vp = g[s];
    
    ivl_statement_t bb = vp.bb;
    const signal_acnt_list_t& in = vp.in;
    const signal_acnt_list_t& out = vp.out;
    const signal_acnt_list_t& gen = vp.gen;
    const signal_acnt_list_t& kill = vp.kill;
    
    std::string label_content;
    label_content.reserve(256);

    //auto print_flow_value = [&label_content](const signal_acnt_list_t& fv_set){
    //    for (const auto &e: fv_set)
    //    {
    //        auto sig = e.m_sig;
    //        const char *sig_name = ivl_signal_basename(sig);

    //        label_content.append(sig_name);
    //        label_content.append("[");

    //        for (const auto &num : e.m_acnts)
    //        {
    //            label_content.append(std::to_string(num));
    //            label_content.append(",");
    //        }

    //        label_content.append("],\n");
    //    }
    //};

    /*
      format:
      in:\n
      a[1,2,3,],\n
      b[1,],\n
    */
    label_content.append("in:\n");
    print_flow_value(in, label_content);
    
    label_content.append("gen:\n");
    print_flow_value(gen, label_content);
    
    label_content.append("kill:\n");
    print_flow_value(kill, label_content);
    
    label_content.append("out:\n");
    print_flow_value(out, label_content);

    //if (s==m_entry || s==m_exit)
    if (bb==nullptr)
    {
        // 0 is entry
        fprintf(p_file, "V%lld [label=\"%s\n%s\"];\n", (long long)s,
                s==0 ? "Entry" : "Exit", label_content.c_str());
    } else {
        assert(bb);
        fprintf(p_file, "V%lld [label=\"lineno=%d\n%s\"];\n", (long long)s,
                (int)ivl_stmt_lineno(bb), label_content.c_str());
    }
};

// clang-format on
#endif
