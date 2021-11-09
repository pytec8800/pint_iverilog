#include "analysis_sensitive.h"
#include "use_def.h"
#include "syntax_tree.h"
#include "dom_tree.h"

void gen_deletable_signals_result(ud_solver_t &u_solver, ud_solver_t &d_solver,
                                  std::set<ivl_signal_t> &removal_signals) {
    // auto entry = u_solver.get_entry();
    // int re = 0;

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

    // {
    //     std::set<ivl_signal_t> tmp;
    //     for (const auto &sig : d_signals) { // intersect
    //         auto search = signals.find(sig);
    //         if (search != signals.end()) { // found
    //             tmp.emplace(sig);
    //         }
    //     }
    //     signals = std::move(tmp);
    // }

    TimingEnd;

    // 2. construct stmt-vertexdesc map
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

    // 3. find out the rval signal which the coressponding down_graph vertex'in has the same signal.
    //    This rval signal in this vertex is not a sensitive signal.
    const auto &d_cfg = d_solver.get_graph();
    std::unordered_map<ivl_signal_t, std::set<ivl_statement_t>> removal_signals_map;
    std::unordered_map<ivl_signal_t, std::set<ivl_statement_t>> sensitive_signals_map;

    auto vis_find_removal = [&signals, &d_bbid_vertex_map, &removal_signals_map,
                             &sensitive_signals_map,
                             &d_cfg](const typename ud_solver_t::VertexDescriptor &s,
                                     const typename ud_solver_t::graph_t &g) -> int {
        const signal_ud_list_t &gen = g[s].gen;
        for (auto it = gen.begin(); it != gen.end(); it++) {
            // for (const auto &sig : gen) {
            auto sig = it->m_sig;
            // ignore the gen's signal which is not in the singals
            auto search_out = signals.find(sig);
            if (search_out != signals.end()) { // found
                // save the rval signal to removal_signals_map
                if (sensitive_signals_map.find(sig) == sensitive_signals_map.end()) {
                    std::set<ivl_statement_t> stmt_set;
                    stmt_set.emplace(g[s].bb);
                    sensitive_signals_map[sig] = stmt_set;
                } else {
                    sensitive_signals_map[sig].emplace(g[s].bb);
                }

                // save the sensitive removal signal to removal_signals_map
                auto d_vertex = d_bbid_vertex_map[g[s].bb_id];
                const signal_ud_list_t &d_in = d_cfg[d_vertex].in;
                // if the signal is in the down_graph's vertex in signals.
                auto search = d_in.find(sig);
                if (search != d_in.end()) { // found
                    // if signal is not in removal_signals_map
                    if (removal_signals_map.find(sig) == removal_signals_map.end()) {
                        std::set<ivl_statement_t> stmt_set;
                        stmt_set.emplace(g[s].bb);
                        removal_signals_map[sig] = stmt_set;
                    } else {
                        removal_signals_map[sig].emplace(g[s].bb);
                    }
                }
            }
        }
        return 0;
    };

    if (!signals.empty()) {
        u_solver.visit_vertices(vis_find_removal);
    }

    // 4. check if the signal in removal_signals_map can be a removal signal
    for (const auto &sig_stmts : removal_signals_map) {
        if (sensitive_signals_map.find(sig_stmts.first) != sensitive_signals_map.end() &&
            sig_stmts.second.size() == sensitive_signals_map[sig_stmts.first].size()) {
            removal_signals.emplace(sig_stmts.first);
        }
    }

    // debug: print out the removal_singals
    __DebugBegin(4) std::cout
        << "===================the sensitive signal result========================" << std::endl;
    for (const auto sig : removal_signals) {
        const char *sig_name = ivl_signal_basename(sig);
        std::cout << "the sensitive signal which can be removed: " << sig_name
                  << " file: " << ivl_signal_file(sig) << "line: " << ivl_signal_lineno(sig)
                  << std::endl;
    }
    __DebugEnd
}

template <typename T>
void print_signals_info(std::set<T> &sensitive_signals, std::string case_name) {
    for (const auto &sig : sensitive_signals) {
        const char *sig_name = ivl_signal_basename(sig);
        std::cout << case_name << " the signal: " << sig_name << "  file: " << ivl_signal_file(sig)
                  << "  line: " << ivl_signal_lineno(sig) << std::endl;
    }
}

void gen_analysis_sensitive_signals(ud_solver_t &u_solver,
                                    std::set<ivl_signal_t> &analysis_sensitive_signals) {
    // auto entry = u_solver.get_entry();

    const auto &g = u_solver.get_graph();
    auto entry = u_solver.get_entry();
    const signal_ud_list_t &in = g[entry].in;
    for (const auto &defs : in) {
        analysis_sensitive_signals.emplace(defs.m_sig);
    }
    // print_signals_info(analysis_sensitive_signals, "gen_analysis_sensitive_signals");
}

// deletable_nexus = used_sensitive_signals - analysis_sensitive_nexus
void gen_deletable_nexus(std::set<ivl_signal_t> &deletable_signals,
                         std::set<ivl_signal_t> &used_sensitive_signals,
                         std::set<ivl_signal_t> &analysis_sensitive_signals) {
    for (const auto &sig : used_sensitive_signals) {
        auto search = analysis_sensitive_signals.find(sig);
        if (search == analysis_sensitive_signals.end()) { // not found
            deletable_signals.emplace(sig);
        }
    }

    __DebugBegin(4) std::cout
        << "===================the deletalbe signal result========================" << std::endl;
    for (const auto &sig : deletable_signals) {
        const char *sig_name = ivl_signal_basename(sig);
        std::cout << "the deletalbe signal: " << sig_name << "  file: " << ivl_signal_file(sig)
                  << "  line: " << ivl_signal_lineno(sig) << std::endl;
    }
    __DebugEnd
}

void collect_sensitive_nexus(const ivl_statement_t net, std::set<ivl_nexus_t> &sensitive_nexus) {
    if (IVL_ST_WAIT == ivl_statement_type(net)) {
        for (unsigned idx = 0; idx < ivl_stmt_nevent(net); idx += 1) {
            ivl_event_t evt = ivl_stmt_events(net, idx);
            if (evt != 0) {
                pint_event_info_t event_info = pint_find_event(evt);
                if (event_info) {
                    for (unsigned idx = 0; idx < event_info->any_num; idx += 1) {
                        ivl_nexus_t nex = ivl_event_any(event_info->evt, idx);
                        sensitive_nexus.emplace(nex);
                    }
                }
            }
        }
    }
}

// Do dataflow signal sensitivity analysis.
int analysis_signal_sensitivity(const ivl_process_t net,
                                std::set<ivl_signal_t> &deletable_signals) {
    deletable_signals.clear();

    // collect lvalue signals
    // std::set<ivl_signal_t> signals;
    // std::set<ivl_signal_t> nb_signals;
    // collect_lval_signal_t l_sigs_finder(nb_signals, signals);
    // l_sigs_finder(ivl_process_stmt(net), nullptr);

    std::set<ivl_signal_t> all_handle_l_signals;
    // all_handle_l_signals.insert(nb_signals.begin(), nb_signals.end());
    // all_handle_l_signals.insert(signals.begin(), signals.end());

    switch (ivl_process_type(net)) {
    case IVL_PR_ALWAYS: {
        ivl_statement_t stmt = ivl_process_stmt(net); // first statement in process

        ud_solver_t d_solver;
        ud_solver_t u_solver;

        {
            int re;
            TimingBegin("d_solver.create_cfg") re = d_solver.create_cfg(stmt, 1);
            TimingEnd

                // assert(re == 0);
                if (re != 0) return re;

            // dataflow solve
            TimingBegin("d_solver.solve may") re = d_solver.solve<true>(
                definition_gen, null_kill, definition_meet, definition_transfer);
            TimingEnd assert(re == 0);

            // debug
            __DebugBegin(2);
            d_solver.dump_graph("always_may_def_fg");
            __DebugEnd;

            TimingBegin("d_solver.solve must") re =
                d_solver.solve<true>(null_gen, null_kill, definition_meet_anticipate,
                                     definition_transfer /*use set merge*/);
            TimingEnd assert(re == 0);

            // debug
            __DebugBegin(2);
            d_solver.dump_graph("always_must_def_fg");
            __DebugEnd;

            // analysis result
        }

        /* up analysis */
        {
            TimingBegin("u_solver set cfg and clear nodes")
                u_solver.set_graph(d_solver.get_graph(), d_solver.get_entry(), d_solver.get_exit());
            u_solver.clear_dfa();
            TimingEnd

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
            TimingBegin("u_solver.solve may") re =
                u_solver.solve<false>(use_gen, use_kill, use_meet, use_transfer);
            TimingEnd assert(re == 0);

            // debug
            __DebugBegin(2);
            u_solver.dump_graph("always_may_use_fg");
            __DebugEnd;

            // must-use analysis
            TimingBegin("u_solver.solve must");
            re = u_solver.solve<false>(null_gen, null_kill, use_meet_anticipate,
                                       use_transfer /*use set merge*/);
            TimingEnd;
            assert(re == 0);

            // debug
            __DebugBegin(2);
            u_solver.dump_graph("always_must_use_fg");
            __DebugEnd;

            // analysis result
            TimingBegin("gen_remove_sensitive_signal_result");

            gen_deletable_signals_result(u_solver, d_solver, deletable_signals);

            TimingEnd;
        }
        break;
    }
    default:
        break;
    }

    return 0;
}

// Do dataflow signal sensitivity analysis.
int analysis_sensitive(const ivl_process_t net, std::set<ivl_signal_t> &deletable_signals) {
    deletable_signals.clear();
    // the use signals, equal to the original sensitive signals
    std::set<ivl_signal_t> used_sensitive_signals;
    std::set<ivl_signal_t> analysis_sensitive_nexus; // the result sensitive nexus of analysis

    if (ivl_process_type(net) == IVL_PR_ALWAYS) {
        ivl_statement_t stmt = ivl_process_stmt(net); // first statement in process
        ud_solver_t u_solver;

        /* up analysis */
        {
            // 1. create graph
            int re;
            TimingBegin("u_solver.create_cfg") re = u_solver.create_cfg(stmt, 1);
            TimingEnd

                // assert(re == 0);
                if (re != 0) return re;

            __DebugBegin(3);
            auto vis_print_bbid = [](typename ud_solver_t::VertexDescriptor s,
                                     typename ud_solver_t::graph_t &g) -> int {
                fprintf(stderr, "[bbid=%u, stmt-no=%d]\n", g[s].bb_id,
                        g[s].bb ? (int)ivl_stmt_lineno(g[s].bb) : -1);
                return 0;
            };
            fprintf(stderr, "\nu_solver bbid: \n");
            u_solver.visit_vertices(vis_print_bbid);
            __DebugEnd;

            // 2. solve fix points
            // use-define analysis
            TimingBegin("u_solver.solve may") re = u_solver.solve<false>(
                sensitive_gen, sensitive_kill, sensitive_meet, sensitive_transfer);
            TimingEnd assert(re == 0);

            // 3. collect all the use signals which is the original sensitive signals.
            auto collect_all_use_signals = [&used_sensitive_signals](
                typename ud_solver_t::VertexDescriptor s, typename ud_solver_t::graph_t &g) -> int {
                int re = 0;
                const auto &stmt = g[s].bb;
                std::set<ivl_signal_t> use_sigs;
                if (stmt) {
                    re = collect_all_sigs_exclude_lval(stmt, use_sigs);
                }
                for (auto &sig : use_sigs) {
                    used_sensitive_signals.emplace(sig);
                }
                return re;
            };
            u_solver.visit_vertices(collect_all_use_signals);

            // debug
            __DebugBegin(4);
            std::cout << "dump_graph sensitive_fg" << std::endl;
            u_solver.dump_graph("sensitive_fg");
            __DebugEnd;

            // 4. collect the analysis_sensitive_signals which is not include deletable signals
            gen_analysis_sensitive_signals(u_solver, analysis_sensitive_nexus);

            // 5. compute deletable_signals = used_sensitive_signals - analysis_sensitive_nexus
            gen_deletable_nexus(deletable_signals, used_sensitive_signals,
                                analysis_sensitive_nexus);
            TimingEnd;
        }

    }

    return 0;
}
