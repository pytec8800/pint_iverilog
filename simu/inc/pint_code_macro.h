#define DOWN_CHAR_E(dst, src, el)                                                                  \
    if (dst != src) {                                                                              \
        change_flag = edge_char(dst, src);                                                         \
        dst = src;                                                                                 \
        pint_process_event_list(el, change_flag);                                                  \
    }

#define DOWN_CHAR_L(dst, src, ll)                                                                  \
    if (dst != src) {                                                                              \
        dst = src;                                                                                 \
        pint_add_list_to_inactive(ll + 1, (unsigned int)ll[0]);                                    \
    }

#define DOWN_CHAR_D(dst, src, ds, wd)                                                              \
    if (dst != src) {                                                                              \
        dst = src;                                                                                 \
        pint_vcddump_char(dst, wd, ds);                                                            \
    }

#define DOWN_CHAR_EL(dst, src, el, ll)                                                             \
    if (dst != src) {                                                                              \
        change_flag = edge_char(dst, src);                                                         \
        dst = src;                                                                                 \
        pint_process_event_list(el, change_flag);                                                  \
        pint_add_list_to_inactive(ll + 1, (unsigned int)ll[0]);                                    \
    }

#define DOWN_CHAR_ED(dst, src, el, ds, wd)                                                         \
    if (dst != src) {                                                                              \
        change_flag = edge_char(dst, src);                                                         \
        dst = src;                                                                                 \
        pint_process_event_list(el, change_flag);                                                  \
        pint_vcddump_char(dst, wd, ds);                                                            \
    }

#define DOWN_CHAR_LD(dst, src, ll, ds, wd)                                                         \
    if (dst != src) {                                                                              \
        dst = src;                                                                                 \
        pint_add_list_to_inactive(ll + 1, (unsigned int)ll[0]);                                    \
        pint_vcddump_char(dst, wd, ds);                                                            \
    }

#define DOWN_CHAR_ELD(dst, src, el, ll, ds, wd)                                                    \
    if (dst != src) {                                                                              \
        change_flag = edge_char(dst, src);                                                         \
        dst = src;                                                                                 \
        pint_process_event_list(el, change_flag);                                                  \
        pint_add_list_to_inactive(ll + 1, (unsigned int)ll[0]);                                    \
        pint_vcddump_char(dst, wd, ds);                                                            \
    }

#define DOWN_INT_E(dst, src, el, wd)                                                               \
    if (!pint_case_equality_int_ret(dst, src, wd)) {                                               \
        change_flag = edge_int(dst, src, wd);                                                      \
        pint_copy_int(dst, src, wd);                                                               \
        pint_process_event_list(el, change_flag);                                                  \
    }

#define DOWN_INT_L(dst, src, ll, wd)                                                               \
    if (!pint_case_equality_int_ret(dst, src, wd)) {                                               \
        pint_copy_int(dst, src, wd);                                                               \
        pint_add_list_to_inactive(ll + 1, (unsigned int)ll[0]);                                    \
    }

#define DOWN_INT_D(dst, src, ds, wd)                                                               \
    if (!pint_case_equality_int_ret(dst, src, wd)) {                                               \
        pint_copy_int(dst, src, wd);                                                               \
        pint_vcddump_int(dst, wd, ds);                                                             \
    }

#define DOWN_INT_EL(dst, src, el, ll, wd)                                                          \
    if (!pint_case_equality_int_ret(dst, src, wd)) {                                               \
        change_flag = edge_int(dst, src, wd);                                                      \
        pint_copy_int(dst, src, wd);                                                               \
        pint_process_event_list(el, change_flag);                                                  \
        pint_add_list_to_inactive(ll + 1, (unsigned int)ll[0]);                                    \
    }

#define DOWN_INT_ED(dst, src, el, ds, wd)                                                          \
    if (!pint_case_equality_int_ret(dst, src, wd)) {                                               \
        change_flag = edge_int(dst, src, wd);                                                      \
        pint_copy_int(dst, src, wd);                                                               \
        pint_process_event_list(el, change_flag);                                                  \
        pint_vcddump_int(dst, wd, ds);                                                             \
    }

#define DOWN_INT_LD(dst, src, ll, ds, wd)                                                          \
    if (!pint_case_equality_int_ret(dst, src, wd)) {                                               \
        pint_copy_int(dst, src, wd);                                                               \
        pint_add_list_to_inactive(ll + 1, (unsigned int)ll[0]);                                    \
        pint_vcddump_int(dst, wd, ds);                                                             \
    }

#define DOWN_INT_ELD(dst, src, el, ll, ds, wd)                                                     \
    if (!pint_case_equality_int_ret(dst, src, wd)) {                                               \
        change_flag = edge_int(dst, src, wd);                                                      \
        pint_copy_int(dst, src, wd);                                                               \
        pint_process_event_list(el, change_flag);                                                  \
        pint_add_list_to_inactive(ll + 1, (unsigned int)ll[0]);                                    \
        pint_vcddump_int(dst, wd, ds);                                                             \
    }

#define NBDOWN_CHAR(sn, nb, idx)                                                                   \
    if (sn != nb) {                                                                                \
        pint_enqueue_nb_thread_by_idx(idx);                                                        \
    }

#define NBDOWN_INT(sn, nb, idx, wd)                                                                \
    if (!pint_case_equality_int_ret(sn, nb, wd)) {                                                 \
        pint_enqueue_nb_thread_by_idx(idx);                                                        \
    }
