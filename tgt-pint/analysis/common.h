#pragma once
#ifndef __ANALYSIS_COMMON_H__
#define __ANALYSIS_COMMON_H__
// clang-format off

template <typename T> class TD {};
#include <type_traits>

#define DFA_DEBUG 0

#define __DebugBegin(level) if /*constexpr*/ (level == DFA_DEBUG) {
#define __DebugEnd }

#define __DBegin(level) if /*constexpr*/ (DFA_DEBUG level) {
#define __DEnd }

#define DFA_NOT_SUPPORT_STMT(net)                                                                  \
    do {                                                                                           \
        /*if(DFA_DEBUG>0) {*/                                                                      \
        auto code = ivl_statement_type(net);                                                       \
        fprintf(stderr, "%s:%d: DFA stmt not support. type=%d. file:%s:%d\n", ivl_stmt_file(net),  \
                ivl_stmt_lineno(net), code, __FILE__, __LINE__);                                   \
        /*}*/                                                                                      \
    } while (false)

#define DFA_NOT_SUPPORT_EXPR(net)                                                                  \
    do {                                                                                           \
        /*if(DFA_DEBUG>0) {*/                                                                      \
        auto code = ivl_expr_type(net);                                                            \
        fprintf(stderr, "%s:%d: DFA expr not support. type=%d. file:%s:%d\n", ivl_expr_file(net),  \
                ivl_expr_lineno(net), code, __FILE__, __LINE__);                                   \
        /*}*/                                                                                      \
    } while (false)

inline void DFA_not_support(ivl_statement_t net, const char *file, int lineno, const char *msg) {
    auto code = ivl_statement_type(net);
    fprintf(stderr, "%s:%d: DFA stmt %s. type=%d. file:%s:%d\n", ivl_stmt_file(net),
            ivl_stmt_lineno(net), msg, code, file, lineno);
}
inline void DFA_not_support(ivl_expr_t net, const char *file, int lineno, const char *msg) {
    auto code = ivl_expr_type(net);
    fprintf(stderr, "%s:%d: DFA expr %s. type=%d. file:%s:%d\n", ivl_expr_file(net),
            ivl_expr_lineno(net), msg, code, file, lineno);
}

inline void DFA_error_msg(ivl_statement_t net, const char *file, int lineno, const char *msg) {
    fprintf(stderr, "%s:%d: [DFA]:%s file:%s:%d\n", ivl_stmt_file(net), ivl_stmt_lineno(net), msg,
            file, lineno);
}

inline void DFA_error_msg(ivl_expr_t net, const char *file, int lineno, const char *msg) {
    fprintf(stderr, "%s:%d: [DFA]:%s file:%s:%d\n", ivl_expr_file(net), ivl_expr_lineno(net), msg,
            file, lineno);
}

#define DFA_NOT_SUPPORT(net)                                                                       \
    do {                                                                                           \
        /*if(DFA_DEBUG>0) {*/                                                                      \
        DFA_not_support(net, __FILE__, __LINE__, "not support");                                   \
        /*}*/                                                                                      \
    } while (false)

#define DFA_NOT_VISIT(net)                                                                         \
    do {                                                                                           \
        /*if(DFA_DEBUG>0) {*/                                                                      \
        DFA_not_support(net, __FILE__, __LINE__, "not visited");                                   \
        /*}*/                                                                                      \
    } while (false)

#ifdef UNUSED
#elif defined(__GNUC__)
#define UNUSED(x) UNUSED_##x __attribute__((unused))
#elif defined(__LCLINT__)
#define UNUSED(x) /* @unused@ */ x
#else
#define UNUSED(x) x
#endif

#define __Concat2(x, y) x##y
#define Concat2(x, y) __Concat2(x, y)

// Graph Asserts
//#define MustHasOneEdge(u, v, g)

#define NetAssert(e, net)                                                                          \
    do {                                                                                           \
        if (nullptr != net) {                                                                      \
            bool r = !!(e);                                                                        \
            if (!r) {                                                                              \
                DFA_not_support(net, __FILE__, __LINE__, "Assertion Failed!");                     \
                assert(r);                                                                         \
            }                                                                                      \
        } else {                                                                                   \
            assert((e));                                                                           \
        }                                                                                          \
    } while (false)

// unreachable execution path
#define NetAssertWrongPath(net)                                                                    \
    do {                                                                                           \
        if (nullptr != net) {                                                                      \
            DFA_error_msg(net, __FILE__, __LINE__, "ERROR: unreachable path!");                    \
            std::exit(-1);                                                                         \
        }                                                                                          \
    } while (false)

//// O = A U (B-C), O/A/B/C has its own storage space.
// template<typename T>
// inline
// void setop_extract_merge(
//    std::set<T>& O,
//    const std::set<T>& A,
//    const std::set<T>& B,
//    const std::set<T>& C)
//{
//    O = A;
//    for(const auto& b: B){
//        if(C.find(b) == C.end()){ // not found
//            O.emplace(b);
//        }
//    }
//}

// C = C U (A-B), C/A/B has its own storage space.
// setop_fem
template <typename T>
inline void setop_extract(std::set<T> &C, const std::set<T> &A, const std::set<T> &B) {
    for (const auto &a : A) {
        if (B.find(a) == B.end()) { // not found
            C.emplace(a);
        }
    }
}

// C = C U (A ∩ B), C/A/B has its own storage space.
// setop_fmi = set operation that fused merge and intersection
template <typename T>
void setop_intersect(std::set<T> &C, const std::set<T> &A, const std::set<T> &B) {
    for (const auto &b : B) {
        auto search = A.find(b);
        if (search != A.end()) { // found
            C.emplace(b);
        }
    }
}

// C = C U (A U B), C/A/B has its own storage space.
// setop_fmm
template <typename T> void setop_merge(std::set<T> &C, const std::set<T> &A, const std::set<T> &B) {
    C.insert(A.begin(), A.end());
    C.insert(B.begin(), B.end());
}

// C = A - B, C/A/B has its own storage space.
template <typename T>
inline void set_subtract(std::set<T> &C, const std::set<T> &A, const std::set<T> &B) {
    C = A;
    for (const auto &b : B) {
        auto search = C.find(b);
        if (search != C.end()) { // found
            C.erase(search);
        }
    }
}

//// C = A U B
// template<typename T>
// void set_merge(std::set<T>& C, const std::set<T>& A, const std::set<T>& B)
//{
//    ;
//}

//// C = A n B
// template<typename T>
// void set_intersect(std::set<T>& C, const std::set<T>& A, const std::set<T>& B)
//{
//    ;
//}

#include "priv.h"

// for STL debug
inline void _Print_Set(const std::set<ivl_signal_t> &A) {
    fprintf(stderr, "{");
    for (const auto &a : A) {
        fprintf(stderr, "%s,", ivl_signal_basename(a));
    }
    fprintf(stderr, "}\n");
}

#define DBGPrintSet(A)                                                                             \
    do {                                                                                           \
        fprintf(stderr, "%s:", #A);                                                                \
        _Print_Set(A);                                                                             \
    } while (0)

// time measure
#include <ctime>

class timestamp_t {
  public:
    explicit timestamp_t(const char *s = nullptr, FILE *f = stderr) : msg_(s), file_(f) {
        start_ = std::clock();
    }

    ~timestamp_t() noexcept {
        double dur_sec = (std::clock() - start_ + 0.0) / CLOCKS_PER_SEC;
        if (msg_) {
            fprintf(file_, "%s elapsed: %.4lf sec\n", msg_, dur_sec);
        }
    }

  public:
    std::clock_t start_;
    const char *msg_;
    FILE *file_;
};

/*
#define TimingBegin(msg) \
    { timestamp_t __time(msg)
#define TimingEnd }
*/

#define TimingBegin(msg)
#define TimingEnd
// print color

// clang-format on
#endif
