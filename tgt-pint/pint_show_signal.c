/******************************************************************************** Std length -100 */
int dump_symbol_id = 0;
unsigned pint_sig_num = 0;
unsigned pint_arr_num = 0;
vector<pint_sig_info_t> pint_sig_info_needs_dump;
map<ivl_nexus_t, pint_sig_info_t> pint_nex_to_sig_info;
map<ivl_signal_t, pint_sig_info_t> pint_sig_to_sig_info;
//  Note: when the key is -1, means can not get arr_idx from this signal
map<ivl_signal_t, int> pint_sig_to_arr_idx;

static void dump_simu_var(ivl_signal_t sig, string symbol);

// pli dut internal signal visited by tb
map<string, ivl_signal_port_t> pint_sig_to_port_type;

bool pint_check_nexus_strengh(ivl_nexus_t net) {
    if (!net) {
        return false;
    }
    unsigned ptr_num = ivl_nexus_ptrs(net);
    ivl_signal_t sig;
    ivl_lpm_t lpm;
    ivl_net_logic_t logic;
    ivl_switch_t switch_;
    ivl_net_const_t con;
    for (unsigned i = 0; i < ptr_num; i++) {
        ivl_nexus_ptr_t ptr = ivl_nexus_ptr(net, i);
        if (((ivl_nexus_ptr_drive0(ptr) != IVL_DR_STRONG) &&
             (ivl_nexus_ptr_drive0(ptr) != IVL_DR_HiZ)) ||
            ((ivl_nexus_ptr_drive1(ptr) != IVL_DR_STRONG) &&
             (ivl_nexus_ptr_drive1(ptr) != IVL_DR_HiZ))) {
            if (NULL != (sig = ivl_nexus_ptr_sig(ptr))) {
                PINT_UNSUPPORTED_ERROR(ivl_signal_file(sig), ivl_signal_lineno(sig),
                                       "drive strength");
            } else if (NULL != (lpm = ivl_nexus_ptr_lpm(ptr))) {
                PINT_UNSUPPORTED_ERROR(ivl_lpm_file(lpm), ivl_lpm_lineno(lpm), "drive strength");
            } else if (NULL != (logic = ivl_nexus_ptr_log(ptr))) {
                PINT_UNSUPPORTED_ERROR(ivl_logic_file(logic), ivl_logic_lineno(logic),
                                       "drive strength");
            } else if (NULL != (switch_ = ivl_nexus_ptr_switch(ptr))) {
                PINT_UNSUPPORTED_ERROR(ivl_switch_file(switch_), ivl_switch_lineno(switch_),
                                       "drive strength");
            } else if (NULL != (con = ivl_nexus_ptr_con(ptr))) {
                PINT_UNSUPPORTED_ERROR(ivl_const_file(con), ivl_const_lineno(con),
                                       "drive strength");
            } else {
                continue;
            }
            return true;
        }
    }
    return false;
}

/******************************************************************************** Std length -100 */
//  Show sig_info
void pint_parse_signal(ivl_signal_t net, unsigned is_root_scope, int dump_scope_flag) {
    if (!net)
        return;
    if (pint_sig_to_sig_info.count(net)) {
        // need save basename for dumpvar
        pint_sig_info_t sig_info = pint_sig_to_sig_info[net];
        if (sig_info) {
            if (sig_info->is_dump_var && dump_scope_flag) {
                dump_simu_var(net, sig_info->dump_symbol);
            }
        }
        return;
    }

    ivl_signal_type_t sig_type = ivl_signal_type(net);
    if ((sig_type == IVL_SIT_TRI0) || (sig_type == IVL_SIT_TRI1)) {
        PINT_UNSUPPORTED_WARN(ivl_signal_file(net), ivl_signal_lineno(net),
                              "tri0 or tri1 perfoms z if no one drives it,we are fixing it now");
    }

    if (pint_pli_mode) {
        string sig_base_name = ivl_signal_basename(net);
        if (sig_base_name.substr(0, 6) == "_pint_") {
            string port_name = sig_base_name.substr(6, -1);
            size_t pos = port_name.find('_');
            string port = port_name.substr(0, pos);
            string name = port_name.substr(pos + 1, -1);
            if ((port == "output") || (port == "input")) {
                size_t id_pos = name.find_last_of('_');
                string actual_name = name.substr(0, id_pos);
                string id = name.substr(id_pos + 1, -1);
                int port_id = atoi(id.c_str());
                // printf("---crosss module name[%s] port_id[%d] port_type[%s]\n",
                //        actual_name.c_str(), port_id, port.c_str());
                if (port == "output") {
                    pint_output_port_name_id[actual_name] = port_id;
                    pint_cross_module_out_port_num++;
                    pint_sig_to_port_type[actual_name] = IVL_SIP_OUTPUT;
                } else if (port == "input") {
                    pint_input_port_name_id[actual_name] = port_id;
                    pint_cross_module_in_port_num++;
                    pint_sig_to_port_type[actual_name] = IVL_SIP_INPUT;
                }
            } else {
                printf("ERROR: pli signal port type is not valid.\n");
            }
        }
    }
    pint_sig_info_t sig_info = pint_gen_new_sig_info(net);
    if (pint_pli_mode) {
        sig_info->is_interface = is_root_scope;
        map<string, ivl_signal_port_t>::iterator it =
            pint_sig_to_port_type.find(ivl_signal_basename(net));
        if (it != pint_sig_to_port_type.end()) {
            if ((NULL == ivl_signal_scope(net)->parent->parent) &&
                (0 == strcmp(ivl_scope_basename(ivl_signal_scope(net)), "DUT"))) {
                sig_info->is_interface = 1;
                sig_info->port_type = it->second;

                if (IVL_SIP_OUTPUT == sig_info->port_type || IVL_SIP_INOUT == sig_info->port_type) {
                    sig_info->output_port_id = pint_output_port_name_id[ivl_signal_basename(net)];
                }

                if (IVL_SIP_INPUT == sig_info->port_type || IVL_SIP_INOUT == sig_info->port_type) {
                    sig_info->input_port_id = pint_input_port_name_id[ivl_signal_basename(net)];
                }
            }
        }
    }
    ivl_signal_t sig_arr = pint_parse_signal_arr_info(net);
    if (sig_arr) { // is arr
        unsigned arr_words = ivl_signal_array_count(sig_arr);
        unsigned i;
        sig_info->sig = sig_arr;
        sig_info->arr_words = arr_words;
        sig_info->const_idx = new set<unsigned>;
        sig_info->sensitives = new map<unsigned, pint_sig_sen_t>;
        sig_info->force_infos = new map<unsigned, pint_force_info_t>;
        pint_show_sig_name(sig_info->sig_name, sig_arr, 1);
        pint_parse_signal_con_val_info(sig_info, sig_arr);
        for (i = 0; i < arr_words; i++) {
            if (pint_check_nexus_strengh(ivl_signal_nex(sig_arr, i))) {
                break;
            }
        }
        pint_update_sig_info_map(sig_info, sig_arr);
        pint_arr_num++;
    } else {
        sig_info->sig = net;
        if (dump_scope_flag) {
            pint_parse_signal_dump_info(sig_info, net);
        }
        pint_show_sig_name(sig_info->sig_name, net, 0);
        pint_parse_signal_con_val_info(sig_info, net);
        pint_check_nexus_strengh(ivl_signal_nex(net, 0));
        pint_update_sig_info_map(sig_info, net);
        pint_sig_num++;
    }
}

pint_sig_info_t pint_gen_new_sig_info(ivl_signal_t net) { // Set default value
    pint_sig_info_t sig_info = new struct pint_sig_info_s;
    sig_info->sig = NULL; // needs update
    sig_info->width = ivl_signal_width(net);
    sig_info->arr_words = 1; // needs update
    sig_info->is_const = 0;  // needs update
    sig_info->has_nb = 0;
    sig_info->is_local = ivl_signal_local(net);
    sig_info->is_func_io = 0;
    sig_info->is_dump_var = 0; // needs update
    sig_info->is_interface = 0;
    sig_info->port_type = IVL_SIP_NONE;
    sig_info->output_port_id = -1;
    sig_info->input_port_id = -1;
    sig_info->local_tmp_idx = 0;
    sig_info->dump_symbol = NULL; // needs update
    sig_info->sig_name = "";      // needs update
    sig_info->value = "";         // needs update
    sig_info->sensitive = NULL;   // needs update
    sig_info->force_info = NULL;  // needs update
    return sig_info;
}

ivl_signal_t pint_parse_signal_arr_info(ivl_signal_t net) {
    //  Ret: is_arr, return addr of arr; not_arr, return NULL
    unsigned arr_words = ivl_signal_array_count(net);
    if (arr_words > 1)
        return net;
    ivl_nexus_t nex = ivl_signal_nex(net, 0);
    if (!nex)
        return NULL;
    unsigned nptrs = ivl_nexus_ptrs(nex);
    ivl_nexus_ptr_t ptr;
    ivl_signal_t sig;
    unsigned i;
    for (i = 0; i < nptrs; i++) {
        ptr = ivl_nexus_ptr(nex, i);
        sig = ivl_nexus_ptr_sig(ptr);
        if (sig) {
            if (ivl_signal_array_count(sig) > 1) {
                return sig;
            }
        }
    }
    return NULL;
}

void pint_parse_arr_nex(set<unsigned> &ret, pint_sig_info_t sig_info) {
    unsigned arr_words = sig_info->arr_words;
    ivl_signal_t sig = sig_info->sig;
    ivl_nexus_t nex;
    unsigned i;
    for (i = 0; i < arr_words; i++) {
        nex = ivl_signal_nex(sig, i);
        if (nex) {
            ret.insert(i);
        }
    }
}

void pint_check_const_delay(vector<ivl_net_const_t> con) {
    for (unsigned i = 0; i < con.size(); i++) {
        ivl_expr_t delay_expr = ivl_const_delay(con[i], 0);
        if (delay_expr) {
            ivl_expr_type_t type_delay = ivl_expr_type(delay_expr);
            if (type_delay == IVL_EX_NUMBER) {
                if (pint_get_ex_number_value_lu(delay_expr, true) > 0) {
                    PINT_UNSUPPORTED_ERROR(ivl_const_file(con[i]), ivl_const_lineno(con[i]),
                                           "assign to constant in assign delay");
                }
            } else {
                assert(0);
            }
        }
    }
}

void pint_parse_signal_con_val_info(pint_sig_info_t sig_info, ivl_signal_t net) {
    unsigned arr_words = ivl_signal_array_count(net);
    unsigned width = ivl_signal_width(net);
    unsigned i;
    if (arr_words > 1) {
        set<unsigned> has_nex;
        pint_parse_arr_nex(has_nex, sig_info);
        unsigned nex_num = has_nex.size();
        if (!nex_num) {
            sig_info->value = "{}"; //  Note: Need each element has the same init value
        } else {
            sig_bit_val_t bit_arr = pint_get_signal_type(net);
            sig_bit_val_t bit_i;
            vector<ivl_net_const_t> con;
            map<unsigned, string> spcl_init;
            string init;
            pint_gen_siganl_init_value(init, bit_arr, width);
            for (i = 0; i < arr_words; i++) {
                if (!has_nex.count(i)) {
                    continue;
                }
                con = pint_parse_nex_con_info(ivl_signal_nex(net, i));
                if (con.size()) {
                    pint_check_const_delay(con);
                    sig_info->const_idx->insert(i);
                    pint_gen_const_value(spcl_init[i], con);
                    continue;
                }
                if (bit_arr == BIT_Z) {
                    bit_i = pint_get_sig_info_type(net, i);
                    if (bit_i != BIT_Z) {
                        pint_gen_siganl_init_value(spcl_init[i], bit_i, width);
                    }
                }
            }
            if (!spcl_init.size()) {
                sig_info->value = "{}";
            } else {
                string buff = "{";
                for (i = 0; i < arr_words; i++) {
                    if (!spcl_init.count(i)) {
                        buff += init;
                    } else {
                        buff += spcl_init[i];
                    }
                    buff += ", ";
                }
                buff.erase(buff.end() - 2, buff.end());
                buff += "}";
                sig_info->value = buff;
            }
        }
    } else {
        vector<ivl_net_const_t> con = pint_parse_nex_con_info(ivl_signal_nex(net, 0));
        if (con.size()) {
            pint_check_const_delay(con);
            sig_info->is_const = 1;
            pint_gen_const_value(sig_info->value, con);
        } else {
            sig_bit_val_t bit_arr = pint_get_sig_info_type(net, 0);
            sig_info->is_const = 0;
            pint_gen_siganl_init_value(sig_info->value, bit_arr, width);
        }
    }
}

vector<ivl_net_const_t> pint_parse_nex_con_info(ivl_nexus_t nex) {
    vector<ivl_net_const_t> res;
    if (!nex) {
        return res;
    }
    unsigned nptrs = ivl_nexus_ptrs(nex);
    ivl_nexus_ptr_t ptr;
    ivl_net_const_t con;
    unsigned i;
    for (i = 0; i < nptrs; i++) {
        ptr = ivl_nexus_ptr(nex, i);
        con = ivl_nexus_ptr_con(ptr);
        if (con) {
            res.push_back(con);
        }
    }
    return res;
}

void pint_gen_const_value(string &buff, vector<ivl_net_const_t> net) {
    vector<const char *> bits;
    double real_val;
    unsigned is_support = 1;
    unsigned width;

    for (unsigned i = 0; i < net.size(); i++) {
        if (i == 0) {
            width = ivl_const_width(net[i]);
        } else {
            if (width != ivl_const_width(net[i])) {
                PINT_UNSUPPORTED_ERROR(ivl_const_file(net[i]), ivl_const_lineno(net[i]),
                                       "multi const width mismatch");
            }
        }
        switch (ivl_const_type(net[i])) {
        case IVL_VT_BOOL:
        case IVL_VT_LOGIC:
            bits.push_back(ivl_const_bits(net[i]));
            break;
        case IVL_VT_REAL:
            if (width <= 32) {
                real_val = ivl_const_real(net[i]);
                buff += string_format("%f", real_val);
                return;
            } else {
                is_support = 0;
            }
            break;
        case IVL_VT_STRING:
            bits.push_back(ivl_const_bits(net[i]));
            break;
        default:
            is_support = 0;
            break;

            if (!is_support) {
                PINT_UNSUPPORTED_ERROR(ivl_const_file(net[i]), ivl_const_lineno(net[i]),
                                       "const type %u", ivl_const_type(net[i]));
                return;
            }
        }
    }

    if (bits.size() == 1) {
        pint_show_ex_number(buff, buff, NULL, NULL, NULL, bits[0], width, EX_NUM_DATA,
                            ivl_const_file(net[0]), ivl_const_lineno(net[0]));
    }

    if (bits.size() > 1) {
        char *muxed_bits = (char *)malloc(width);
        memset(muxed_bits, 'z', width);
        for (unsigned i = 0; i < bits.size(); i++) {
            for (unsigned bit_i = 0; bit_i < width; bit_i++) {
                if ((bits[i][bit_i] != '0') && (bits[i][bit_i] != '1') && (bits[i][bit_i] != 'x') &&
                    (bits[i][bit_i] != 'z')) {
                    PINT_UNSUPPORTED_ERROR(ivl_const_file(net[i]), ivl_const_lineno(net[i]),
                                           "const bits %c", bits[i][bit_i]);
                    return;
                }

                if (muxed_bits[bit_i] != bits[i][bit_i]) {
                    if (muxed_bits[bit_i] == 'z') {
                        muxed_bits[bit_i] = bits[i][bit_i];
                    } else if (bits[i][bit_i] == 'z') {
                    } else {
                        muxed_bits[bit_i] = 'x';
                    }
                }
            }
        }
        pint_show_ex_number(buff, buff, NULL, NULL, NULL, muxed_bits, width, EX_NUM_DATA,
                            ivl_const_file(net[0]), ivl_const_lineno(net[0]));
        free(muxed_bits);
    }
}

void pint_gen_siganl_init_value(string &buff, sig_bit_val_t bit_arr, unsigned width) {
    unsigned bit_h = (bit_arr & 2) ? 1 : 0;
    unsigned bit_l = bit_arr & 1;
    if (width <= 4) { // Ret(for example, val = x, width = 1): 0x11
        if (bit_h) {
            bit_h = ((1 << width) - 1) << 4;
        }
        if (bit_l) {
            bit_l = (1 << width) - 1;
        }
        buff += string_format("0x%x", bit_h | bit_l);
    } else { // Ret(for example, val = z, width = 5): {0x0, 0x1f}
        unsigned cnt = (width + 0x1f) >> 5;
        unsigned width_last = width & 0x1f; // [0, 31], warning: 0 means 32 => [1, 32]
        unsigned value = bit_l;
        unsigned i, j;
        buff += "{";
        for (i = 0; i < 2; i++, value = bit_h) {
            if (value) {
                for (j = 0; j < cnt - 1; j++) {
                    buff += "0xffffffff, ";
                }
                if (width_last) { // width_last [1, 31]
                    buff += string_format("0x%x, ", (1 << width_last) - 1);
                } else { // width_last = 32
                    buff += "0xffffffff, ";
                }
            } else {
                for (j = 0; j < cnt; j++) {
                    buff += "0x0, ";
                }
            }
        }
        buff.erase(buff.end() - 2, buff.end());
        buff += "}";
    }
}

sig_bit_val_t pint_get_sig_info_type(ivl_signal_t sig, unsigned word) {
    //  Will seek following nexus
    sig_bit_val_t ret = pint_get_signal_type(sig);
    ivl_nexus_t nex = ivl_signal_nex(sig, word);
    if (ret != BIT_Z || !nex)
        return ret;
    unsigned nptrs = ivl_nexus_ptrs(nex);
    ivl_nexus_ptr_t ptr;
    ivl_signal_t sig_i;
    unsigned i;
    for (i = 0; i < nptrs; i++) {
        ptr = ivl_nexus_ptr(nex, i);
        sig_i = ivl_nexus_ptr_sig(ptr);
        if (sig_i) {
            ret = pint_get_signal_type(sig_i);
            if (ret != BIT_Z) {
                return ret;
            }
        }
    }
    return BIT_Z;
}

sig_bit_val_t pint_get_signal_type(ivl_signal_t sig) {
    //  Only check current signal
    switch (ivl_signal_type(sig)) {
    case IVL_SIT_TRI0: // set 0
        return BIT_0;
    case IVL_SIT_TRI1: // set 1
        return BIT_1;
    case IVL_SIT_TRI: // set z
    case IVL_SIT_TRIAND:
    case IVL_SIT_TRIOR:
    case IVL_SIT_UWIRE:
        return BIT_Z;
    default: // set x
        return BIT_X;
    }
}

static unsigned int scope_layers(const char *scope_name) {
    int i, len;
    unsigned int layer_num = 0;
    len = strlen(scope_name);
    for (i = 0; i < len; i++) {
        if (scope_name[i] == '.')
            layer_num++;
    }
    return layer_num;
}

static bool should_dumped(ivl_signal_t sig) {
    if (pint_simu_info.need_dump) {
        ivl_scope_t target_scope = ivl_signal_scope(sig);
        const char *target_sig_basename = ivl_signal_basename(sig);
        char target_scope_name[256];
        char target_sig_name[256];
        sprintf(target_scope_name, "%s.", ivl_scope_name(target_scope));
        sprintf(target_sig_name, "%s%s", target_scope_name, target_sig_basename);
        unsigned int dump_layer, base_layers, target_layers;
        target_layers = scope_layers(target_scope_name);
        for (unsigned i = 0; i < pint_simu_info.dump_scopes.size(); i++) {
            dump_layer = pint_simu_info.dump_scopes[i].layer;
            //            printf("the layer is: %d\n", pint_simu_info.dump_scopes[i].layer);
            for (unsigned j = 0; j < pint_simu_info.dump_scopes[i].scopes.size(); j++) {
                char base_scope_name[256];
                sprintf(base_scope_name, "%s.",
                        ivl_scope_name(pint_simu_info.dump_scopes[i].scopes[j]));
                base_layers = scope_layers(base_scope_name);
                //                printf("base scope is: %s, target scope is: %s\n",
                //                base_scope_name, target_scope_name);
                if (strstr(target_scope_name, base_scope_name)) {
                    if (dump_layer == 0) {
                        return true;
                    } else if (target_layers - base_layers < dump_layer) {
                        return true;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            }
        }
        for (unsigned i = 0; i < pint_simu_info.dump_signals.size(); i++) {
            const char *base_sig_basename = ivl_signal_basename(pint_simu_info.dump_signals[i]);
            char base_sig_name[256];
            sprintf(base_sig_name, "%s.%s",
                    ivl_scope_name(ivl_signal_scope(pint_simu_info.dump_signals[i])),
                    base_sig_basename);
            //            printf("base singal is: %s target_sig_name: %s\n", base_sig_name,
            //            target_sig_name);
            if (strcmp(target_sig_name, base_sig_name) == 0) {
                //                printf("dumped base singal is: %s\n",
                //                ivl_signal_name(pint_simu_info.dump_signals[i]));
                return true;
            }
        }
        return false;
    } else {
        return false;
    }
}

static char *get_new_dump_symbol() {
    char *dump_symbol;
    int s_id = dump_symbol_id;
    int q, r, i, tmp;
    int symbol_size = (2 + 2 * (int)(log1p(dump_symbol_id) / log1p(126.0 - 33.0))) * 2;
    dump_symbol_id++;
    dump_symbol = (char *)malloc(symbol_size);
    i = 0;
    do {
        q = s_id / (126 - 33 + 1); // 94
        r = s_id % 94;
        s_id = (s_id - r) / 94;
        tmp = r + 33;
        if ((tmp == '"') || (tmp == '\\') || (tmp == '\'')) {
            dump_symbol[i++] = '\\';
        }
        dump_symbol[i++] = tmp;
    } while (q != 0);
    dump_symbol[i] = 0;
    return dump_symbol;
}

static void dump_simu_var(ivl_signal_t sig, string symbol) {
    unsigned int sig_width = ivl_signal_width(sig);
    const char *type = "";
    const char *sig_name = ivl_signal_basename(sig);
    string tmp_buf;

    switch (ivl_signal_type(sig)) {
    case IVL_SIT_REG:
        type = "reg";
        if (ivl_signal_integer(sig))
            type = "integer";
        break;
    case IVL_SIT_TRI:
    case IVL_SIT_TRI0:
    case IVL_SIT_TRI1:
        type = "wire";
        break;
    case IVL_SIT_UWIRE:
        type = "uwire";
        break;
    default:
        type = "wire";
        break;
    }

    if (sig_width == 1) {
        tmp_buf = string_format("$var %s %d %s %s $end", type, sig_width, symbol.c_str(), sig_name);
    } else {
        tmp_buf = string_format("$var %s %d %s %s [%d:0] $end", type, sig_width, symbol.c_str(),
                                sig_name, sig_width - 1);
    }
    simu_header_buff.push_back(tmp_buf);

    return;
}

void pint_parse_signal_dump_info(pint_sig_info_t sig_info, ivl_signal_t net) {
    if (sig_info->arr_words > 1)
        return;
    if (sig_info->is_const)
        return;
    if (sig_info->is_local)
        return;
    unsigned is_dump = 0;
    ivl_nexus_t nex = ivl_signal_nex(net, 0);
    if (!nex) {
        is_dump = should_dumped(net);
    } else {
        unsigned nptrs = ivl_nexus_ptrs(nex);
        ivl_nexus_ptr_t ptr;
        ivl_signal_t sig;
        unsigned i;
        for (i = 0; i < nptrs; i++) {
            ptr = ivl_nexus_ptr(nex, i);
            sig = ivl_nexus_ptr_sig(ptr);
            if (sig) {
                if (should_dumped(sig)) {
                    is_dump = 1;
                    break;
                }
            }
        }
    }
    if (is_dump) {
        sig_info->is_dump_var = 1;
        sig_info->dump_symbol = get_new_dump_symbol();
        dump_symbol_id++;
        dump_simu_var(net, sig_info->dump_symbol);
    }
}

void pint_show_sig_name(string &sig_name, ivl_signal_t net, bool is_arr) {
    if (PINT_SIMPLE_SIG_NAME_EN && !pint_pli_mode) {
        if (!is_arr) {
            sig_name = string_format("SIG%u", pint_sig_num);
        } else {
            sig_name = string_format("ARR%u", pint_arr_num);
        }
    } else {
        sig_name = ivl_signal_name_c(net);
        unsigned len = sig_name.length();
        unsigned i;
        for (i = 0; i < len; i++) {
            if ((sig_name[i] < '0') || ((sig_name[i] > '9') && (sig_name[i] < 'A')) ||
                ((sig_name[i] > 'Z') && (sig_name[i] < 'a')) || (sig_name[i] > 'z')) {
                sig_name[i] = '_';
            }
        }
    }
}

pint_sig_sen_t pint_gen_new_sig_sensitive() {
    pint_sig_sen_t ret = new struct pint_sig_sen_s;
    ret->lpm_num = 0;
    ret->pos_evt_num = 0;
    ret->neg_evt_num = 0;
    ret->any_evt_num = 0;
    ret->optimized = 0;
    return ret;
}

pint_sig_sen_t pint_gen_new_sig_sensitive(pint_sig_info_t sig_info, unsigned word) {
    pint_sig_sen_t ret = pint_gen_new_sig_sensitive();
    if (sig_info->arr_words == 1) {
        sig_info->sensitive = ret;
    } else {
        map<unsigned, pint_sig_sen_t> &tgt = *(sig_info->sensitives);
        tgt[word] = ret;
    }
    return ret;
}

void pint_add_nex_into_sig_map(ivl_nexus_t nex, pint_sig_info_t sig_info) {
    if (pint_nex_to_sig_info.count(nex)) {
        if (pint_nex_to_sig_info[nex] != sig_info) {
            const char *sn = pint_find_signal(nex)->sig_name.c_str();
            printf("Error!  --find nex link to two sig_info. SIG: %s\n", sn);
        }
    } else {
        pint_nex_to_sig_info[nex] = sig_info;
    }
}

void pint_add_signal_into_sig_map(ivl_signal_t sig, pint_sig_info_t sig_info) {
    if (pint_sig_to_sig_info.count(sig)) {
        if (pint_sig_to_sig_info[sig] != sig_info) {
            const char *sn = pint_find_signal(sig)->sig_name.c_str();
            printf("Error!  --find signal link to two sig_info. SIG: %s\n", sn);
        }
    } else {
        pint_sig_to_sig_info[sig] = sig_info;
    }
}

void pint_add_arr_idx_into_sig_map(ivl_signal_t sig, int arr_idx) {
    if (pint_sig_to_arr_idx.count(sig)) {
        if (pint_sig_to_arr_idx[sig] != arr_idx) {
            const char *sn = pint_find_signal(sig)->sig_name.c_str();
            printf("Error!  --find signal has two arr_idx. SIG: %s\n", sn);
        }
    } else {
        pint_sig_to_arr_idx[sig] = arr_idx;
    }
}

void pint_update_sig_info_map(pint_sig_info_t sig_info, ivl_signal_t sig_arr) {
    //  Note: do not care whether the sig_info needs dump at here
    pint_sig_info_needs_dump.push_back(sig_info);
    unsigned arr_words = sig_info->arr_words;
    unsigned nptrs;
    ivl_nexus_t nex;
    ivl_nexus_ptr_t ptr;
    ivl_signal_t sig;
    unsigned i, j;

    pint_add_signal_into_sig_map(sig_arr, sig_info);
    pint_add_arr_idx_into_sig_map(sig_arr, arr_words == 1 ? 0 : -1);
    for (i = 0; i < arr_words; i++) {
        nex = ivl_signal_nex(sig_arr, i);
        if (!nex) {
            continue;
        }
        pint_add_nex_into_sig_map(nex, sig_info);
        nptrs = ivl_nexus_ptrs(nex);
        for (j = 0; j < nptrs; j++) {
            ptr = ivl_nexus_ptr(nex, j);
            sig = ivl_nexus_ptr_sig(ptr);
            if (sig && sig != sig_arr) {
                pint_add_signal_into_sig_map(sig, sig_info);
                if (sig_info->is_interface) {
                    if ((NULL == ivl_signal_scope(sig)->parent->parent) &&
                        (0 == strcmp(ivl_scope_basename(ivl_signal_scope(sig)), "DUT"))) {
                        sig_info->port_type = ivl_signal_port(sig);

                        if (IVL_SIP_OUTPUT == sig_info->port_type ||
                            IVL_SIP_INOUT == sig_info->port_type) {
                            map<string, int>::iterator it =
                                pint_output_port_name_id.find(ivl_signal_basename(sig));
                            if (it != pint_output_port_name_id.end()) {
                                sig_info->output_port_id = it->second;
                                // printf("---output signame[%s]:port_id[%d]\n",
                                // ivl_signal_basename(sig),
                                // sig_info->output_port_id);
                            }
                        }

                        if (IVL_SIP_INPUT == sig_info->port_type ||
                            IVL_SIP_INOUT == sig_info->port_type) {
                            map<string, int>::iterator it =
                                pint_input_port_name_id.find(ivl_signal_basename(sig));
                            if (it != pint_input_port_name_id.end()) {
                                sig_info->input_port_id = it->second;
                                // printf("---input signame[%s]:port_id[%d]\n",
                                // ivl_signal_basename(sig), sig_info->input_port_id);
                            }
                        }
                    }
                }
                pint_add_arr_idx_into_sig_map(sig, i);
            }
        }
    }
}
/******************************************************************************** Std length -100 */
//  Optimize sen
/*
signal_optimize_info:   <type, <lpm_idx/evt_idx, <sen_word>>>
    type:   0, lpm; 1, pos_evt; 2, any_evt; 3, neg_evt;
*/
map<unsigned, map<unsigned, set<unsigned>>> signal_optimize_info;

void pint_parse_optimize_sen(pint_sig_sen_t sen, unsigned word) {
    unsigned num = 0;
    unsigned rd;
    set<unsigned>::iterator i;
    for (rd = 0; rd < 4; rd++) {
        set<unsigned> *src;
        map<unsigned, set<unsigned>> &dst = signal_optimize_info[rd];
        switch (rd) {
        case 0:
            num = sen->lpm_num;
            src = &(sen->lpm_list);
            break;
        case 1:
            num = sen->pos_evt_num;
            src = &(sen->pos_evt_list);
            break;
        case 2:
            num = sen->any_evt_num;
            src = &(sen->any_evt_list);
            break;
        case 3:
            num = sen->neg_evt_num;
            src = &(sen->neg_evt_list);
            break;
        }
        if (num) {
            for (i = src->begin(); i != src->end(); i++) {
                dst[*i].insert(word);
            }
        }
    }
}

void pint_optimize_signal_one_sen_thread(map<unsigned, pint_sig_sen_t> &tgt, unsigned thread,
                                         unsigned type) {
    set<unsigned> &words = signal_optimize_info[type][thread];
    set<unsigned>::iterator i;
    unsigned word;
    pint_sig_sen_t sen;
    for (i = words.begin(); i != words.end(); i++) {
        word = *i;
        if (word != 0xffffffff) {
            sen = tgt[word];
            switch (type) {
            case 0:
                sen->lpm_num--;
                sen->lpm_list.erase(thread);
                break;
            case 1:
                sen->pos_evt_num--;
                sen->pos_evt_list.erase(thread);
                break;
            case 2:
                sen->any_evt_num--;
                sen->any_evt_list.erase(thread);
                break;
            case 3:
                sen->neg_evt_num--;
                sen->neg_evt_list.erase(thread);
                break;
            }
        }
    }
    if (!words.count(0xffffffff)) {
        if (!tgt.count(0xffffffff)) {
            tgt[0xffffffff] = pint_gen_new_sig_sensitive();
        }
        sen = tgt[0xffffffff];
        switch (type) {
        case 0:
            sen->lpm_num++;
            sen->lpm_list.insert(thread);
            break;
        case 1:
            sen->pos_evt_num++;
            sen->pos_evt_list.insert(thread);
            break;
        case 2:
            sen->any_evt_num++;
            sen->any_evt_list.insert(thread);
            break;
        case 3:
            sen->neg_evt_num++;
            sen->neg_evt_list.insert(thread);
            break;
        }
    }
}

void pint_optimize_sen_one_type(map<unsigned, pint_sig_sen_t> &tgt, unsigned arr_words,
                                unsigned type) {
    map<unsigned, set<unsigned>> *src = &(signal_optimize_info[type]);
    map<unsigned, set<unsigned>>::iterator i;
    set<unsigned> *sens; //  thread sen set
    unsigned num;        //  thread sen word num
    for (i = src->begin(); i != src->end(); i++) {
        sens = &(i->second);
        num = sens->size();
        if (num <= 1) {
            continue;
        }
        if (sens->count(0xffffffff) || num > 3 || num > arr_words / 3) {
            pint_optimize_signal_one_sen_thread(tgt, i->first, type);
        }
    }
}

void pint_sig_info_sen_delete_static_only(pint_sig_info_t sig_info) 
{
    pint_sig_sen_t sensitive = sig_info->sensitive;
    if(sensitive) {
        if(sensitive->pos_evt_num > 0) {
            for(auto iter = sensitive->pos_evt_list.begin(); iter != sensitive->pos_evt_list.end();) {
                pint_event_info_t evt_info = pint_find_event(*iter);
                if(evt_info) {
                    if(evt_info->wait_count == evt_info->static_wait_count) {
                        iter = sensitive->pos_evt_list.erase(iter);
                        sensitive->pos_evt_num--;
                        sensitive->optimized = 1;
                    }
                    else{
                      iter++;
                    }
                }
                else {
                  iter++;
                }
            }
        }
        if(sensitive->any_evt_num > 0) {
            for(auto iter = sensitive->any_evt_list.begin(); iter != sensitive->any_evt_list.end();) {
                pint_event_info_t evt_info = pint_find_event(*iter);
                if(evt_info) {
                    if(evt_info->wait_count == evt_info->static_wait_count) {
                        iter = sensitive->any_evt_list.erase(iter);
                        sensitive->any_evt_num--;
                        sensitive->optimized = 1;
                    }
                    else{
                      iter++;
                    }                    
                }
                else {
                  iter++;
                }                
            }
        }
        if(sensitive->neg_evt_num > 0) {
            for(auto iter = sensitive->neg_evt_list.begin(); iter != sensitive->neg_evt_list.end();) {
                pint_event_info_t evt_info = pint_find_event(*iter);
                if(evt_info) {
                    if(evt_info->wait_count == evt_info->static_wait_count) {
                        iter = sensitive->neg_evt_list.erase(iter);
                        sensitive->neg_evt_num--;
                        sensitive->optimized = 1;
                    }
                    else{
                      iter++;
                    }                    
                }
                else {
                  iter++;
                }                
            }
        }                
    }
    return;
}

void pint_sig_info_sen_optimize(pint_sig_info_t sig_info) {
    unsigned arr_words = sig_info->arr_words;
    if (arr_words == 1) {
        //delete the static_only event
        pint_sig_info_sen_delete_static_only(sig_info);
        return;
    }
    if(sig_info->sensitives->size() <= 1) {
        return;
    }
    map<unsigned, pint_sig_sen_t> &tgt = *(sig_info->sensitives);
    map<unsigned, pint_sig_sen_t>::iterator i;
    for (i = tgt.begin(); i != tgt.end(); i++) {
        pint_parse_optimize_sen(i->second, i->first);
    }
    unsigned rd;
    for (rd = 0; rd < 4; rd++) {
        pint_optimize_sen_one_type(tgt, arr_words, rd);
    }
    signal_optimize_info.clear();

}

/******************************************************************************** Std length -100 */
/*
Note:   1. The operation must been done before 'optimizing sen':
            pint_event_add_list
            pint_lpm_add_list
            show_monitor_exprs
            show_process_add_list
            pint_parse_all_force_shadow_lpm
            pint_dump_force_lpm
        2. The operation must been done before 'remove_sig_info_no_need_dump':
            pint_parse_all_force_shadow_lpm
            pint_parse_all_force_const_lpm
            show_all_func
            show_all_task
            pint_dump_process
*/
//  Dump sig_info
void pint_remove_sig_info_no_need_dump();
void pint_dump_signal() {
    pint_remove_sig_info_no_need_dump();
    pint_sig_info_t sig_info;
    unsigned num = pint_sig_info_needs_dump.size();
    unsigned i;
    string enqueue_callback_list;
    PINT_LOG_HOST("SIG: %u\n", num);
    signal_list += "\n//  signal list\n";
    enqueue_callback_list +=
        string_format("pint_thread pli_callback_func_array[%d];\n",
                      pint_input_port_num + pint_inout_port_num + pint_cross_module_in_port_num);
    enqueue_callback_list += "void enqueue_change_callback_list(void) {\n";
    enqueue_callback_list += "\tfor (int i = 0; i < pint_input_port_change_num; i++) {\n";
    // enqueue_callback_list += "\t\tprintf(\"---num[%d], id[%d]\\n\", pint_input_port_change_num,
    // pint_input_port_change_id[i]);\n";
    enqueue_callback_list += "\t\tpint_enqueue_thread(pli_callback_func_array[pint_input_port_"
                             "change_id[i]], ACTIVE_Q);\n";
    enqueue_callback_list += "\t}\n}\n\n";

    callback_list += "\n//  pli interface input signal change callback\n";
    signal_force += "\n//  force variable\n";
    for (i = 0; i < num; i++) {
        sig_info = pint_sig_info_needs_dump[i];
        if (pint_pli_mode) {
            pint_show_signal_callback_list_define(callback_list, sig_info);
        }
        pint_sig_info_sen_optimize(sig_info);
        pint_show_signal_define(signal_buff, inc_buff, sig_info, 0);
        pint_show_signal_sen_list_define(signal_list, inc_buff, sig_info);
        pint_show_signal_force_define(signal_force, inc_buff, sig_info);
    }
    callback_list += enqueue_callback_list;

    for (map<ivl_nexus_t, int>::iterator it = Multi_drive_record.begin();
         it != Multi_drive_record.end(); it++) {
        unsigned multi_dri_num = it->second;
        ivl_nexus_t nex = it->first;
        pint_sig_info_t sig_info_tmp = pint_find_signal(nex);
        unsigned width = sig_info_tmp->width;
        unsigned single_item_len = (width <= 4) ? 1 : ((width + 31) / 32 * 2 * 4);
        unsigned multi_drive_table_len = 2 * sizeof(unsigned) + multi_dri_num * (single_item_len);
        multi_drive_table_len = (multi_drive_table_len + 3) / 4;
        string nexus_name =
            string_format("%s_%d", sig_info_tmp->sig_name.c_str(), pint_get_arr_idx_from_nex(nex));
        signal_buff += string_format(
            "unsigned %s_mdrv_tbl[%d] __attribute__((section(\".builtin_simu_buffer\")));\n",
            nexus_name.c_str(), multi_drive_table_len);
        inc_buff += string_format("extern unsigned %s_mdrv_tbl[%d];\n", nexus_name.c_str(),
                                  multi_drive_table_len);
        global_init_buff +=
            string_format("%s_mdrv_tbl[0] = %d;\n%s_mdrv_tbl[1] = %d;\n", nexus_name.c_str(),
                          multi_dri_num, nexus_name.c_str(), width);
        global_init_buff += Multi_drive_init[nex];
    }
}

void pint_remove_sig_info_no_need_dump() {
    vector<pint_sig_info_t> list_tmp;
    pint_sig_info_t sig_info;
    unsigned num = pint_sig_info_needs_dump.size();
    unsigned i;
    for (i = 0; i < num; i++) {
        sig_info = pint_sig_info_needs_dump[i];
        if (sig_info->is_func_io) {
            continue;
        }
        if (sig_info->is_local && sig_info->arr_words == 1 && !(sig_info->is_const)) {
            continue;
        }
        list_tmp.push_back(sig_info);
    }
    pint_sig_info_needs_dump = list_tmp;
}

void pint_show_signal_define(string &buff, string &dec_buff, pint_sig_info_t sig_info,
                             unsigned is_nb) {
    unsigned width = sig_info->width;
    unsigned arr_words = sig_info->arr_words;
    const char *sn = sig_info->sig_name.c_str();
    const char *nb_info = is_nb ? "_nb" : "";
    const char *output_info =
        (pint_pli_mode && sig_info->is_interface &&
         (IVL_SIP_OUTPUT == sig_info->port_type || IVL_SIP_INOUT == sig_info->port_type) && !is_nb)
            ? " __attribute__((section(\".simu.dut.output\")))"
            : "";

    if (arr_words == 1) { //  signal
        if (width <= 4) {
            dec_buff += string_format("extern unsigned char %s%s;\n", sn, nb_info);
            buff += string_format("unsigned char %s%s%s = ", sn, nb_info, output_info);
        } else {
            unsigned cnt2 = ((width + 0x1f) >> 5) * 2;
            dec_buff += string_format("extern unsigned int  %s%s[%u];\n", sn, nb_info, cnt2);
            buff += string_format("unsigned int  %s%s[%u]%s = ", sn, nb_info, cnt2, output_info);
        }
        //  Note: the size of string_format's buffer is limited
        buff += sig_info->value + ";\n";

        if (pint_pli_mode && sig_info->is_interface &&
            (IVL_SIP_INPUT == sig_info->port_type || IVL_SIP_INOUT == sig_info->port_type) &&
            !is_nb) {
            if (width <= 4) {
                dec_buff += string_format("extern unsigned char %s_h2d;\n", sn);
                buff += string_format(
                    "unsigned char %s_h2d __attribute__((section(\".simu.dut.input\")));\n", sn);
            } else {
                unsigned cnt2 = ((width + 0x1f) >> 5) * 2;
                dec_buff += string_format("extern unsigned int  %s_h2d[%u];\n", sn, cnt2);
                buff += string_format(
                    "unsigned int  %s_h2d[%u] __attribute__((section(\".simu.dut.input\")));\n", sn,
                    cnt2);
            }
        }
    } else {
        if (sig_info->value != "{}") {
            if (width <= 4) {
                dec_buff +=
                    string_format("extern unsigned char %s%s[%u];\n", sn, nb_info, arr_words);
                buff += string_format("unsigned char %s%s[%u] = ", sn, nb_info, arr_words);
            } else {
                unsigned cnt2 = ((width + 0x1f) >> 5) * 2;
                dec_buff += string_format("extern unsigned int  %s%s[%u][%u];\n", sn, nb_info,
                                          arr_words, cnt2);
                buff +=
                    string_format("unsigned int  %s%s[%u][%u] = ", sn, nb_info, arr_words, cnt2);
            }
            buff += sig_info->value + ";\n";
        } else {
            if (width <= 4) {
                dec_buff +=
                    string_format("extern unsigned char %s%s[%u];\n", sn, nb_info, arr_words);
                buff += string_format("unsigned char %s%s[%u]" ARRAY_BSS_SEC ";\n", sn, nb_info,
                                      arr_words);
            } else {
                unsigned cnt2 = ((width + 0x1f) >> 5) * 2;
                dec_buff += string_format("extern unsigned int  %s%s[%u][%u];\n", sn, nb_info,
                                          arr_words, cnt2);
                buff += string_format("unsigned int  %s%s[%u][%u]" ARRAY_BSS_SEC ";\n", sn, nb_info,
                                      arr_words, cnt2);
            }

            sig_bit_val_t bit_arr = pint_get_signal_type(sig_info->sig);
            if (pint_array_init_num) {
                array_init_buff += string_format(", {(void*)%s%s, (pint_bit4_t)%d, %u, %u}",
                                                 sig_info->sig_name.c_str(), nb_info, bit_arr,
                                                 sig_info->width, arr_words);
            } else {
                array_init_buff += string_format(
                    "\nstruct pint_array_info array_info[]" ARRAY_INFO_DATA_SEC
                    " = {{(void*)%s%s, (pint_bit4_t)%d, %u, %u}",
                    sig_info->sig_name.c_str(), nb_info, bit_arr, sig_info->width, arr_words);
            }
            pint_array_init_num++;
        }
    }
}

unsigned pint_sen_rm_invalid_evt_in_one_list(unsigned &num, set<unsigned> &list) {
    set<unsigned>::iterator i;
    vector<unsigned> rm;
    unsigned rm_num = 0;
    unsigned j;
    for (i = list.begin(); i != list.end(); i++) {
        if (!pint_is_evt_needs_dump(*i)) {
            rm_num++;
            rm.push_back(*i);
        }
    }
    if (rm_num) {
        for (j = 0; j < rm_num; j++) {
            list.erase(rm[j]);
        }
        num -= rm_num;
    }
    return rm_num;
}

unsigned pint_sen_rm_invalid_evt(pint_sig_sen_t sen) {
    unsigned ret = 0;
    ret += pint_sen_rm_invalid_evt_in_one_list(sen->pos_evt_num, sen->pos_evt_list);
    ret += pint_sen_rm_invalid_evt_in_one_list(sen->any_evt_num, sen->any_evt_list);
    ret += pint_sen_rm_invalid_evt_in_one_list(sen->neg_evt_num, sen->neg_evt_list);
    return ret;
}

struct pint_sen_ret_s {
    unsigned lpm_num;
    unsigned pos_num;
    unsigned any_num;
    unsigned neg_num;
    string lpm_buff;
    string pos_buff;
    string any_buff;
    string neg_buff;
};
typedef struct pint_sen_ret_s *pint_sen_ret_t;

void pint_parse_arr_one_sen_evt_list(string &buff, set<unsigned> &list, unsigned word) {
    set<unsigned>::iterator i;
    for (i = list.begin(); i != list.end(); i++) {
        buff += string_format(", (pint_thread)%u, pint_move_event_%u", word, *i);
    }
}

void pint_parse_arr_sen_info(pint_sen_ret_t ret, pint_sig_sen_t sen, unsigned word) {
    if (!sen) {
        return;
    }
    ret->lpm_num += sen->lpm_num;
    ret->pos_num += sen->pos_evt_num;
    ret->any_num += sen->any_evt_num;
    ret->neg_num += sen->neg_evt_num;
    set<unsigned>::iterator i;
    unsigned id;
    for (i = sen->lpm_list.begin(); i != sen->lpm_list.end(); i++) {
        id = *i + pint_event_num;
        ret->lpm_buff +=
            string_format(", (pint_thread)%u, (pint_thread)%u, pint_lpm_%u", id, word, id);
    }
    pint_parse_arr_one_sen_evt_list(ret->pos_buff, sen->pos_evt_list, word);
    pint_parse_arr_one_sen_evt_list(ret->any_buff, sen->any_evt_list, word);
    pint_parse_arr_one_sen_evt_list(ret->neg_buff, sen->neg_evt_list, word);
}

void pint_parse_sig_one_sen_evt_list(string &buff, set<unsigned> &list) {
    set<unsigned>::iterator i;
    for (i = list.begin(); i != list.end(); i++) {
        buff += string_format(", pint_move_event_%u", *i);
    }
}

/*
Note: even all the sen event can be removed, e_list still needs define. because evt_info->
wait_count maybe changed in 'show_process', thus sig_info may not know evt can be removed in
'show_process'.
*/
void pint_show_signal_sen_list_define(string &buff, string &dec_buff, pint_sig_info_t sig_info) {
    const char *sn = sig_info->sig_name.c_str();
    unsigned arr_words = sig_info->arr_words;
    unsigned evt_rm = 0;
    if (arr_words == 1) {
        pint_sig_sen_t sen = sig_info->sensitive;
        if (sen) {
            set<unsigned>::iterator i;
            unsigned id;
            evt_rm = pint_sen_rm_invalid_evt(sen);
            if (sen->lpm_num) {
                dec_buff += string_format("extern pint_thread l_list_%s[];\n", sn);
                buff +=
                    string_format("pint_thread l_list_%s[] = {(pint_thread)%u", sn, sen->lpm_num);
                for (i = sen->lpm_list.begin(); i != sen->lpm_list.end(); i++) {
                    id = *i + pint_event_num;
                    buff += string_format(", (pint_thread)%u, pint_lpm_%u", id, id);
                }
                buff += "};\n";
            }
            if (sen->pos_evt_num || sen->neg_evt_num || sen->any_evt_num || evt_rm || sen->optimized) {
                  // pint_signal_has_static_list(sig_info) ) {
                string evt_buff;
                pint_parse_sig_one_sen_evt_list(evt_buff, sen->pos_evt_list);
                pint_parse_sig_one_sen_evt_list(evt_buff, sen->any_evt_list);
                pint_parse_sig_one_sen_evt_list(evt_buff, sen->neg_evt_list);
                string s_list_str;
                if(pint_signal_has_static_list(sig_info)) {
                  s_list_str = "s_list_" + sig_info->sig_name;
                }else {
                  s_list_str = "NULL";
                }

                unsigned total_evt_num = sen->pos_evt_num + sen->any_evt_num + sen->neg_evt_num;
                dec_buff += string_format("extern pint_thread e_list_%s[];\n", sn);
                buff += string_format(
                    "pint_thread e_list_%s[] = {(pint_thread)%s, (pint_thread)%u, (pint_thread)%u, (pint_thread)%u, (pint_thread)%u",
                    sn, s_list_str.c_str(), total_evt_num, sen->pos_evt_num, sen->any_evt_num, sen->neg_evt_num);
                buff += evt_buff;
                buff += "};\n";
            }
        }
    } else if (sig_info->sensitives->size()) {
        struct pint_sen_ret_s sen_ret = {0, 0, 0, 0, "", "", "", ""};
        pint_sen_ret_t ret = &sen_ret;
        map<unsigned, pint_sig_sen_t>::iterator i;
        for (i = sig_info->sensitives->begin(); i != sig_info->sensitives->end(); i++) {
            evt_rm += pint_sen_rm_invalid_evt(i->second);
        }
        for (i = sig_info->sensitives->begin(); i != sig_info->sensitives->end(); i++) {
            pint_parse_arr_sen_info(ret, i->second, i->first);
        }
        if (ret->lpm_num) {
            dec_buff += string_format("extern pint_thread l_list_%s[];\n", sn);
            buff += string_format("pint_thread l_list_%s[] = {(pint_thread)%u", sn, ret->lpm_num);
            buff += ret->lpm_buff;
            buff += "};\n";
        }
        if (ret->pos_num || ret->any_num || ret->neg_num || evt_rm) {
            dec_buff += string_format("extern pint_thread e_list_%s[];\n", sn);
            buff += string_format(
                "pint_thread e_list_%s[] = {(pint_thread)%u, (pint_thread)%u, (pint_thread)%u", sn,
                ret->pos_num, ret->any_num, ret->neg_num);
            buff += ret->pos_buff;
            buff += ret->any_buff;
            buff += ret->neg_buff;
            buff += "};\n";
        }
    }
    if (sig_info->sens_process_list.size()) {
        dec_buff += string_format("extern unsigned p_list_%s[];\n", sig_info->sig_name.c_str());
        buff += string_format("unsigned p_list_%s[] = {%d", sig_info->sig_name.c_str(),
                              sig_info->sens_process_list.size());
        for (unsigned ii = 0; ii < sig_info->sens_process_list.size(); ii++) {
	          buff += string_format(", %d", sig_info->sens_process_list[ii]);
        }
        buff += "};\n";
    }
}

void pint_show_signal_callback_list_define(string &buff, pint_sig_info_t sig_info) {
    unsigned width = sig_info->width;
    unsigned arr_words = sig_info->arr_words;
    const char *sn = sig_info->sig_name.c_str();

    if (sig_info->is_interface &&
        (IVL_SIP_INPUT == sig_info->port_type || IVL_SIP_INOUT == sig_info->port_type)) {
        if (arr_words == 1) {
            buff += string_format("void %s_change_callback(void) {\n", sn);
            unsigned has_sen = pint_signal_has_addl_opt_when_updt(sig_info);
            if (has_sen) {
                buff += "\tunsigned change_flag;\n";
                string pre_fix = width > 4 ? "" : "&";
                buff += string_format("\tpint_copy_signal_common_edge(change_flag,%s%s_h2d,%u,%u,%"
                                      "s%s,%u,%u,%u,false);\n",
                                      pre_fix.c_str(), sn, width, 0, pre_fix.c_str(), sn, width, 0,
                                      width);
                buff += "\tif(change_flag != 0){\n";
                if (pint_signal_has_p_list(sig_info)) {
                    buff += pint_gen_sig_p_list_code(sig_info) + "\n";
                }
                if (pint_signal_has_sen_lpm(sig_info)) {
                    buff += pint_gen_sig_l_list_code(sig_info, (unsigned)0) + "\n";
                }
                if (pint_signal_has_sen_evt(sig_info)) {
                    buff += pint_gen_sig_e_list_code(sig_info, "change_flag", (unsigned)0) + "\n";
                }
                pint_vcddump_var(sig_info, &buff);
                buff += "\t}\n";
            } else {
                if (width <= 4) {
                    buff += string_format("\t%s = %s_h2d;\n", sn, sn);
                } else {
                    buff += string_format(
                        "\tfor (int tmp_i = 0; tmp_i < %u; tmp_i++) {\n"
                        "\t\t((unsigned*)%s)[tmp_i] = ((unsigned*)%s_h2d)[tmp_i];\n\t}\n",
                        (width + 31) / 32 * 2, sn, sn);
                }
            }
            buff += "}\n\n";
            global_init_buff +=
                string_format("    pli_callback_func_array[%d] = %s_change_callback;\n",
                              sig_info->input_port_id, sn);
        } else {
            PINT_UNSUPPORTED_WARN(ivl_signal_file(sig_info->sig), ivl_signal_lineno(sig_info->sig),
                                  "interface signal is arr.");
        }
    }
}

void pint_sig_info_gen_force_var_define(string &buff, string &dec_buff, pint_sig_info_t sig_info,
                                        pint_force_info_t force_info, unsigned word) {
    if (!force_info) {
        return;
    }
    string sig_name = pint_get_force_sig_name(sig_info, word);
    const char *sn = sig_name.c_str();
    if (force_info->type == FORCE_NET) {
        unsigned width = sig_info->width;
        if (force_info->part_force) { //  has part force/release
            set<unsigned>::iterator iter;
            string str_type;
            string str_len;
            string value_fmask;
            string value_umask;
            string str_idx;
            if (width <= 4) {
                unsigned value = ((unsigned)1 << width) - 1;
                value |= value << 4;
                str_type = "unsigned char ";
                value_fmask = "0x0";
                value_umask = string_format("0x%x", value);

            } else if (width <= 32) {
                str_type = "unsigned int  ";
                value_fmask = "0x0";
                if (width != 32) {
                    value_umask = string_format("0x%x", ((unsigned)1 << width) - 1);
                } else {
                    value_umask = "0xffffffff";
                }
            } else {
                unsigned cnt = (width + 0x1f) >> 5;
                unsigned mod = width & 0x1f; // [0, 31], 0 means 32
                unsigned i;
                str_type = "unsigned int  ";
                str_len = string_format("[%u]", cnt);
                value_fmask = "{";
                value_umask = "{";
                for (i = 1; i < cnt; i++) {
                    value_fmask += "0x0, ";
                    value_umask += "0xffffffff, ";
                }
                value_fmask += "0x0}";
                if (mod) {
                    value_umask += string_format("0x%x}", ((unsigned)1 << mod) - 1);
                } else {
                    value_umask += "0xffffffff}";
                }
            }
            dec_buff += "extern " + str_type + "umask_" + sig_name + str_len + ";\n";
            buff += str_type + "umask_" + sig_name + str_len + " = " + value_umask + ";\n";

            for (iter = force_info->force_idx.begin(); iter != force_info->force_idx.end();
                 iter++) {
                str_idx = string_format("%u_", *iter + FORCE_THREAD_OFFSET);
                dec_buff += "extern " + str_type + "fmask_" + str_idx + sig_name + str_len + ";\n";
                buff += str_type + "fmask_" + str_idx + sig_name + str_len + " = " + value_fmask +
                        ";\n";
            }
        } else {
            dec_buff += string_format("extern unsigned char force_en_%s;\n", sn);
            buff += string_format("unsigned char force_en_%s = 0;\n", sn);
            if (force_info->mult_force) {
                dec_buff += string_format("extern pint_thread   force_thread_%s;\n", sn);
                buff += string_format("pint_thread   force_thread_%s = NULL;\n", sn);
            }
        }
    } else {
        dec_buff += string_format("extern unsigned char force_en_%s;\n", sn);
        buff += string_format("unsigned char force_en_%s = 0;\n", sn);
        if (force_info->mult_force) {
            dec_buff += string_format("extern pint_thread   force_thread_%s;\n", sn);
            buff += string_format("pint_thread   force_thread_%s = NULL;\n", sn);
        }
    }

    if (force_info->assign_idx.size() > 0) {
        dec_buff += string_format("extern unsigned char assign_en_%s;\n", sn);
        buff += string_format("unsigned char assign_en_%s = 0;\n", sn);
        if (force_info->mult_assign) {
            dec_buff += string_format("extern pint_thread   assign_thread_%s;\n", sn);
            buff += string_format("pint_thread   assign_thread_%s = NULL;\n", sn);
        }
    }
}

void pint_show_signal_force_define(string &buff, string &dec_buff, pint_sig_info_t sig_info) {
    unsigned arr_words = sig_info->arr_words;
    if (arr_words == 1) {
        pint_force_info_t force_info = sig_info->force_info;
        if (force_info) {
            pint_sig_info_gen_force_var_define(buff, dec_buff, sig_info, force_info, 0);
        }
    } else {
        map<unsigned, pint_force_info_t>::iterator i;
        for (i = sig_info->force_infos->begin(); i != sig_info->force_infos->end(); i++) {
            pint_sig_info_gen_force_var_define(buff, dec_buff, sig_info, i->second, i->first);
        }
    }
}

/******************************************************************************** Std length -100 */
//  API
pint_sig_info_t pint_find_signal(ivl_signal_t sig) {
    if (!sig) {
        return NULL;
    }
    if (pint_sig_to_sig_info.count(sig)) {
        return pint_sig_to_sig_info[sig];
    } else {
        printf("Error!  --can not find sig_info from sig. name: %s\n", ivl_signal_name(sig));
        return NULL;
    }
}

pint_sig_info_t pint_find_signal(ivl_nexus_t nex) {
    if (!nex) {
        return NULL;
    }
    if (pint_nex_to_sig_info.count(nex)) {
        return pint_nex_to_sig_info[nex];
    } else {
        printf("Error!  --can not find sig_info from nex.\n");
        return NULL;
    }
}

pint_sig_sen_t pint_find_signal_sen(pint_sig_info_t sig_info, unsigned word) {
    if (sig_info->arr_words == 1) {
        return sig_info->sensitive;
    } else {
        map<unsigned, pint_sig_sen_t> &tgt = *(sig_info->sensitives);
        if (tgt.count(word)) {
            return tgt[word];
        } else {
            return NULL;
        }
    }
}

pint_force_info_t pint_find_signal_force_info(pint_sig_info_t sig_info, unsigned word) {
    unsigned arr_words = sig_info->arr_words;
    if (word >= arr_words) {
        assert(0);
    }
    if (arr_words == 1) {
        return sig_info->force_info;
    } else {
        map<unsigned, pint_force_info_t> &tgt = *(sig_info->force_infos);
        if (tgt.count(word)) {
            return tgt[word];
        } else {
            return NULL;
        }
    }
}

void pint_print_sig_sen_info(pint_sig_sen_t sen, unsigned word) {
    if (!sen) {
        return;
    }
    set<unsigned>::iterator i;
    printf("    sen-%u:", word);
    printf("\n        lpm-idx:      ");
    for (i = sen->lpm_list.begin(); i != sen->lpm_list.end(); i++) {
        printf("%u  ", *i + pint_event_num);
    }
    printf("\n        pos_evt-idx:  ");
    for (i = sen->pos_evt_list.begin(); i != sen->pos_evt_list.end(); i++) {
        printf("%u  ", *i);
    }
    printf("\n        any_evt-idx:  ");
    for (i = sen->any_evt_list.begin(); i != sen->any_evt_list.end(); i++) {
        printf("%u  ", *i);
    }
    printf("\n        neg_evt-idx:  ");
    for (i = sen->neg_evt_list.begin(); i != sen->neg_evt_list.end(); i++) {
        printf("%u  ", *i);
    }
    printf("\n");
}

void pint_print_sig_force_info(pint_force_info_t force_info, unsigned word) {
    if (!force_info) {
        return;
    }
    printf("    force-%u:\n", word);
    printf("        type:           %s\n",
           force_info->type == FORCE_NET ? "FORCE_NET" : "FORCE_VAR");
    printf("        mult_force:     %u\n", force_info->mult_force);
    printf("        part_force:     %u\n", force_info->part_force);
}

void pint_print_sig_info_details(pint_sig_info_t sig_info) {
    unsigned arr_words = sig_info->arr_words;
    unsigned i;
    printf(">>  SIG:            %s\n", sig_info->sig_name.c_str());
    printf("    width:          %u\n", sig_info->width);
    printf("    arr_words:      %u\n", sig_info->arr_words);
    printf("    has_nb:         %u\n", sig_info->has_nb);
    printf("    is_local:       %u\n", sig_info->is_local);
    printf("    is_func_io:     %u\n", sig_info->is_func_io);
    printf("    is_dump_var:    %u\n", sig_info->is_dump_var);
    printf("    dump_symbol:    %s\n", sig_info->dump_symbol);
    printf("    value:          %s\n", sig_info->value.c_str());
    if (arr_words == 1) {
        printf("    is_const:      %u\n", sig_info->is_const);
        pint_print_sig_sen_info(sig_info->sensitive, 0);
        pint_print_sig_force_info(sig_info->force_info, 0);
    } else {
        set<unsigned>::iterator j;
        printf("    const-idx:     ");
        for (i = 0, j = sig_info->const_idx->begin(); j != sig_info->const_idx->end(); i++, j++) {
            printf("%u  ", *j);
            if (i == 3) {
                printf("...");
                break;
            }
        }
        printf("\n");
        map<unsigned, pint_sig_sen_t>::iterator k;
        for (i = 0, k = sig_info->sensitives->begin(); k != sig_info->sensitives->end(); i++, k++) {
            pint_print_sig_sen_info(k->second, k->first);
            if (i == 3) {
                printf("    ...\n");
                break;
            }
        }
        map<unsigned, pint_force_info_t>::iterator l;
        for (i = 0, l = sig_info->force_infos->begin(); l != sig_info->force_infos->end();
             i++, l++) {
            pint_print_sig_force_info(l->second, l->first);
            if (i == 3) {
                printf("    ...\n");
                break;
            }
        }
    }
}

unsigned pint_get_arr_idx_from_nex(ivl_nexus_t nex) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    unsigned arr_words = sig_info->arr_words;
    if (arr_words == 1) {
        return 0;
    } else {
        ivl_signal_t sig_arr = sig_info->sig;
        unsigned i;
        for (i = 0; i < arr_words; i++) {
            if (ivl_signal_nex(sig_arr, i) == nex) {
                return i;
            }
        }
        assert(0);
        return 0;
    }
}

unsigned pint_get_arr_idx_from_sig(ivl_signal_t sig) {
    //  Note(next): if sig = sig_arr, ret = 0xffffffff(means can not get arr_idx from sig_arr)
    if (!pint_sig_to_arr_idx.count(sig)) {
        return 0xffffffff;
    } else {
        return pint_sig_to_arr_idx[sig];
    }
}

string pint_get_sig_value_name(pint_sig_info_t sig_info, unsigned word) {
    if (sig_info->arr_words == 1) {
        return sig_info->sig_name;
    } else {
        return sig_info->sig_name + string_format("[%u]", word);
    }
}

string pint_get_force_sig_name(pint_sig_info_t sig_info, unsigned word) {
    if (sig_info->arr_words == 1) {
        return sig_info->sig_name;
    } else {
        return sig_info->sig_name + string_format("_%u", word);
    }
}

string pint_gen_sig_l_list_code(pint_sig_info_t sig_info, unsigned word) {
    const char *sn = sig_info->sig_name.c_str();
    if (sig_info->arr_words == 1) {
        return string_format("\t\tpint_add_list_to_inactive(l_list_%s +1, (unsigned int)l_list_%s[0]);",
                             sn, sn);
    } else {
        return string_format(
            "\t\tpint_add_arr_list_to_inactive(l_list_%s +1, (unsigned int)l_list_%s[0], %u);", sn, sn,
            word);
    }
}

string pint_gen_sig_l_list_code(pint_sig_info_t sig_info, const char *word) {
    const char *sn = sig_info->sig_name.c_str();
    if (sig_info->arr_words == 1) {
        return string_format("\t\tpint_add_list_to_inactive(l_list_%s +1, (unsigned int)l_list_%s[0]);",
                             sn, sn);
    } else {
        return string_format(
            "pint_add_arr_list_to_inactive(\t\tl_list_%s +1, (unsigned int)l_list_%s[0], %s);", sn, sn,
            word);
    }
}

string pint_gen_sig_l_list_code(pint_sig_info_t sig_info, const char *list, unsigned word) {
    if (sig_info->arr_words == 1) {
        return string_format("\t\tpint_add_list_to_inactive(%s +1, (unsigned int)%s[0]);", list, list);
    } else {
        return string_format("\t\tpint_add_arr_list_to_inactive(%s +1, (unsigned int)%s[0], %u);", list,
                             list, word);
    }
}

string pint_gen_sig_e_list_code(pint_sig_info_t sig_info, const char *edge, unsigned word) {
    const char *sn = sig_info->sig_name.c_str();
    if (sig_info->arr_words == 1) {
        return string_format("\t\tpint_process_event_list(e_list_%s, %s);", sn, edge);
    } else {
        return string_format("\t\tpint_process_arr_event_list(e_list_%s, %s, %u);", sn, edge, word);
    }
}

string pint_gen_sig_e_list_code(pint_sig_info_t sig_info, const char *edge, const char *word) {
    const char *sn = sig_info->sig_name.c_str();
    if (sig_info->arr_words == 1) {
        return string_format("\t\tpint_process_event_list(e_list_%s, %s);", sn, edge);
    } else {
        return string_format("\t\tpint_process_arr_event_list(e_list_%s, %s, %s);", sn, edge, word);
    }
}

string pint_gen_sig_e_list_code(pint_sig_info_t sig_info, const char *list, const char *edge,
                                unsigned word) {
    if (sig_info->arr_words == 1) {
        return string_format("\t\tpint_process_event_list(%s, %s);", list, edge);
    } else {
        return string_format("\t\tpint_process_arr_event_list(%s, %s, %u);", list, edge, word);
    }
}

string pint_gen_sig_p_list_code(pint_sig_info_t sig_info) {
    const char *sn = sig_info->sig_name.c_str();
    return string_format("\t\tpint_set_event_flag(p_list_%s);", sn);
}

void pint_ret_signal_const_value(string &buff, pint_sig_info_t sig_info, unsigned word) {
    vector<ivl_net_const_t> con = pint_parse_nex_con_info(ivl_signal_nex(sig_info->sig, word));
    if (!con.size()) {
        return;
    }
    pint_gen_const_value(buff, con);
}

bool pint_sen_has_valid_sen(pint_sig_sen_t sen) {
    if (!sen) {
        return false;
    }
    if (sen->lpm_num || sen->pos_evt_num || sen->neg_evt_num || sen->any_evt_num) {
        return true;
    } else {
        return false;
    }
}

bool pint_signal_has_sen(pint_sig_info_t sig_info, unsigned word) {
    if (sig_info->arr_words == 1) {
        return pint_sen_has_valid_sen(sig_info->sensitive);
    } else if (word == 0xffffffff) {
        return pint_signal_has_sen(sig_info);
    } else {
        if (pint_sen_has_valid_sen(pint_find_signal_sen(sig_info, word)) ||
            pint_sen_has_valid_sen(pint_find_signal_sen(sig_info, 0xffffffff))) {
            return true;
        } else {
            return false;
        }
    }
}

//  Function: check whether the entire sig_info has sen
bool pint_signal_has_sen(pint_sig_info_t sig_info) {
    unsigned arr_words = sig_info->arr_words;
    if (arr_words == 1) {
        return pint_sen_has_valid_sen(sig_info->sensitive);
    } else {
        map<unsigned, pint_sig_sen_t>::iterator i;
        for (i = sig_info->sensitives->begin(); i != sig_info->sensitives->end(); i++) {
            if (pint_sen_has_valid_sen(i->second)) {
                return true;
            }
        }
        return false;
    }
}

bool pint_sen_has_sen_evt(pint_sig_sen_t sen) {
    if (!sen) {
        return false;
    }
    if (sen->pos_evt_num || sen->neg_evt_num || sen->any_evt_num) {
        return true;
    } else {
        return false;
    }
}

bool pint_signal_has_sen_evt(pint_sig_info_t sig_info, unsigned word) {
    if (sig_info->arr_words == 1) {
        return pint_sen_has_sen_evt(sig_info->sensitive);
    } else if (word == 0xffffffff) {
        return pint_signal_has_sen_evt(sig_info);
    } else {
        if (pint_sen_has_sen_evt(pint_find_signal_sen(sig_info, word)) ||
            pint_sen_has_sen_evt(pint_find_signal_sen(sig_info, 0xffffffff))) {
            return true;
        } else {
            return false;
        }
    }
}

//  Function: check whether the entire sig_info has sen evt
bool pint_signal_has_sen_evt(pint_sig_info_t sig_info) {
    unsigned arr_words = sig_info->arr_words;
    if (arr_words == 1) {
        return pint_sen_has_sen_evt(sig_info->sensitive);
    } else {
        map<unsigned, pint_sig_sen_t>::iterator i;
        for (i = sig_info->sensitives->begin(); i != sig_info->sensitives->end(); i++) {
            if (pint_sen_has_sen_evt(i->second)) {
                return true;
            }
        }
        return false;
    }
}

bool pint_signal_has_static_list(pint_sig_info_t sig_info) 
{
    ivl_signal_t sig = sig_info->sig;
    if(signal_s_list_map.count(sig) > 0) {
        return true;
    }
    else {
        return false;
    }
}

bool pint_sen_has_sen_lpm(pint_sig_sen_t sen) {
    if (!sen) {
        return false;
    }
    if (sen->lpm_num) {
        return true;
    } else {
        return false;
    }
}

bool pint_signal_has_sen_lpm(pint_sig_info_t sig_info, unsigned word) {
    if (sig_info->arr_words == 1) {
        return pint_sen_has_sen_lpm(sig_info->sensitive);
    } else if (word == 0xffffffff) {
        return pint_signal_has_sen_lpm(sig_info);
    } else {
        if (pint_sen_has_sen_lpm(pint_find_signal_sen(sig_info, word)) ||
            pint_sen_has_sen_lpm(pint_find_signal_sen(sig_info, 0xffffffff))) {
            return true;
        } else {
            return false;
        }
    }
}

//  Function: check whether the entire sig_info has sen lpm
bool pint_signal_has_sen_lpm(pint_sig_info_t sig_info) {
    unsigned arr_words = sig_info->arr_words;
    if (arr_words == 1) {
        return pint_sen_has_sen_lpm(sig_info->sensitive);
    } else {
        map<unsigned, pint_sig_sen_t>::iterator i;
        for (i = sig_info->sensitives->begin(); i != sig_info->sensitives->end(); i++) {
            if (pint_sen_has_sen_lpm(i->second)) {
                return true;
            }
        }
        return false;
    }
}

bool pint_signal_has_p_list(pint_sig_info_t sig_info) {
    if (sig_info->sens_process_list.size()) {
        return true;
    } else {
        return false;
    }
}

bool pint_signal_has_addl_opt_when_updt(pint_sig_info_t sig_info, unsigned word) {
    if (pint_signal_has_sen(sig_info, word) || sig_info->is_dump_var ||
        pint_signal_has_p_list(sig_info) ||
        (pint_pli_mode && sig_info->is_interface && (IVL_SIP_OUTPUT == sig_info->port_type))) {
        return true;
    } else {
        return false;
    }
}

bool pint_signal_has_addl_opt_when_updt(pint_sig_info_t sig_info) {
    if (pint_signal_has_sen(sig_info) || sig_info->is_dump_var ||
        pint_signal_has_p_list(sig_info) ||
        (pint_pli_mode && sig_info->is_interface && (IVL_SIP_OUTPUT == sig_info->port_type))) {
        return true;
    } else {
        return false;
    }
}

bool pint_signal_is_const(pint_sig_info_t sig_info, unsigned word) {
    if (sig_info->arr_words == 1) {
        if (word == 0) {
            return sig_info->is_const;
        } else {
            assert(0);
        }
    } else {
        return sig_info->const_idx->count(word);
    }
}

bool pint_signal_is_const(ivl_nexus_t nex) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    if (sig_info->arr_words == 1) {
        return sig_info->is_const;
    } else {
        unsigned word = pint_get_arr_idx_from_nex(nex);
        return sig_info->const_idx->count(word);
    }
}

unsigned pint_get_const_value(ivl_nexus_t nex) {
    unsigned nptrs = ivl_nexus_ptrs(nex);
    ivl_nexus_ptr_t ptr;
    ivl_net_const_t con;
    const char *bits;
    unsigned i;
    for (i = 0; i < nptrs; i++) {
        ptr = ivl_nexus_ptr(nex, i);
        con = ivl_nexus_ptr_con(ptr);
        if (con) {
            switch (ivl_const_type(con)) {
            case IVL_VT_BOOL:
            case IVL_VT_LOGIC:
                bits = ivl_const_bits(con);
                return pint_get_bits_value(bits);
            default:
                break;
            }
        }
    }
    printf("Warning  --pint_get_const_value, not find supported const.\n");
    return VALUE_MAX;
}

bool pint_signal_been_forced(pint_sig_info_t sig_info) {
    unsigned arr_words = sig_info->arr_words;
    if (arr_words == 1) {
        return sig_info->force_info ? true : false;
    } else {
        return sig_info->force_infos->size() ? true : false;
    }
}
bool pint_sig_is_force_var(pint_sig_info_t sig_info) {
    if (sig_info->arr_words > 1) {
        return false; //  force_var can be an element of mem
    }
    pint_force_info_t force_info = sig_info->force_info;
    if (!force_info) {
        return false;
    }
    return force_info->type == FORCE_VAR ? true : false;
}

static void pint_sen_add_lpm(pint_sig_sen_t sen, unsigned idx) {
    if (!sen) {
        return;
    }
    if (!sen->lpm_list.count(idx)) {
        sen->lpm_num++;
        sen->lpm_list.insert(idx);
    }
}

void pint_sig_info_add_lpm(pint_sig_info_t sig_info, unsigned word, unsigned lpm_idx) {
    ivl_signal_t sig = sig_info->sig; //  Note: some sig_info has no sig
    if (sig && ivl_scope_type(ivl_signal_scope(sig)) == IVL_SCT_BEGIN) {
        return; //  exclude local signal(is_local = 0) defined in named block
    }
    pint_sig_sen_t sen = pint_find_signal_sen(sig_info, word);
    if (!sen) {
        sen = pint_gen_new_sig_sensitive(sig_info, word);
    }
    pint_sen_add_lpm(sen, lpm_idx);
}

void pint_sig_info_add_lpm(ivl_nexus_t nex, unsigned lpm_idx) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    unsigned word = pint_get_arr_idx_from_nex(nex);
    pint_sig_info_add_lpm(sig_info, word, lpm_idx);
}

static void pint_sen_add_event(pint_sig_sen_t sen, unsigned idx, event_type_t type) {
    if (!sen) {
        return;
    }
    unsigned *num;
    set<unsigned> *list;
    switch (type) {
    case POS:
        num = &(sen->pos_evt_num);
        list = &(sen->pos_evt_list);
        break;
    case ANY:
        num = &(sen->any_evt_num);
        list = &(sen->any_evt_list);
        break;
    case NEG:
        num = &(sen->neg_evt_num);
        list = &(sen->neg_evt_list);
        break;
    default:
        assert(0);
    }
    if (!list->count(idx)) {
        (*num)++;
        list->insert(idx);
    }
}

void pint_sig_info_add_event(ivl_nexus_t nex, unsigned idx, event_type_t type) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    ivl_signal_t sig = sig_info->sig;
    if (sig && ivl_scope_type(ivl_signal_scope(sig)) == IVL_SCT_BEGIN) {
        return; //  exclude local signal(is_local = 0) defined in named block
    }
    unsigned word = pint_get_arr_idx_from_nex(nex);
    pint_sig_sen_t sen = pint_find_signal_sen(sig_info, word);
    if (!sen) {
        sen = pint_gen_new_sig_sensitive(sig_info, word);
    }
    pint_sen_add_event(sen, idx, type);
}

void pint_sig_info_set_const(pint_sig_info_t sig_info, unsigned word, unsigned is_const) {
    if (word >= sig_info->arr_words) {
        assert(0);
    }
    if (sig_info->arr_words == 1) {
        sig_info->is_const = is_const;
    } else if (is_const) {
        sig_info->const_idx->insert(word);
    } else if (sig_info->const_idx->count(word)) {
        sig_info->const_idx->erase(word);
    }
}

void pint_revise_sig_info_to_global(ivl_nexus_t nex) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    sig_info->is_local = 0;
}

void pint_revise_sig_info_to_global(ivl_signal_t sig) {
    pint_sig_info_t sig_info = pint_find_signal(sig);
    sig_info->is_local = 0;
}

pint_force_info_t pint_gen_new_force_info(ivl_signal_t sig, unsigned word, ivl_statement_t stmt,
                                          unsigned is_asgn) {
    pint_sig_info_t sig_info = pint_find_signal(sig);
    unsigned arr_words = sig_info->arr_words;
    if (word >= arr_words) {
        assert(0);
    }
    pint_force_info_t ret = new struct pint_force_info_s;
    ivl_signal_type_t sig_type = ivl_signal_type(sig);
    if (sig_type == IVL_SIT_REG || sig_type == IVL_SIT_NONE) {
        ret->type = FORCE_VAR;
        if (ivl_signal_array_count(sig) > 1) {
            printf("Error!  --syntax error, force/Release of memory word is not valid.\n");
            printf("//  %s:%u\n", ivl_stmt_file(stmt), ivl_stmt_lineno(stmt));
            assert(0);
        }
    } else {
        ret->type = FORCE_NET;
    }
    ret->mult_force = 0;
    ret->part_force = 0; //  Needs update
    ret->scope = ivl_signal_scope(sig);
    if (is_asgn) {
        ret->assign_idx.insert(pint_force_idx[stmt]);
    } else {
        ret->force_idx.insert(pint_force_idx[stmt]);
    }
    if (arr_words == 1) {
        sig_info->force_info = ret;
    } else {
        map<unsigned, pint_force_info_t> &tgt = *(sig_info->force_infos);
        tgt[word] = ret;
    }
    return ret;
}

static void pint_updt_shadow_sig_info_map(pint_sig_info_t dst_info, pint_sig_info_t src_info,
                                          unsigned word) {
    ivl_nexus_t nex = ivl_signal_nex(src_info->sig, word);
    unsigned nptrs = ivl_nexus_ptrs(nex);
    ivl_nexus_ptr_t ptr;
    ivl_signal_t sig;
    unsigned i;
    for (i = 0; i < nptrs; i++) {
        ptr = ivl_nexus_ptr(nex, i);
        sig = ivl_nexus_ptr_sig(ptr);
        if (sig && ivl_signal_port(sig) == IVL_SIP_OUTPUT) {
            //  find sig in sub module that update sig_value
            pint_sig_to_sig_info[sig] = dst_info;
        }
    }
}

pint_sig_info_t pint_gen_force_shadow_sig_info(pint_sig_info_t sig_info, unsigned word) {
    pint_sig_info_t ret = new struct pint_sig_info_s;
    sig_bit_val_t bit_arr = pint_get_sig_info_type(sig_info->sig, word);
    unsigned width = sig_info->width;
    ret->sig = NULL;
    ret->width = width;
    ret->arr_words = 1;
    ret->is_const = 0;
    ret->is_local = 0;
    ret->is_func_io = 0;
    ret->is_dump_var = 0;
    ret->local_tmp_idx = 0;
    ret->dump_symbol = NULL;
    ret->sig_name = "force_" + pint_get_force_sig_name(sig_info, word);
    pint_gen_siganl_init_value(ret->value, bit_arr, width);
    ret->sensitive = NULL;
    ret->force_info = NULL;
    pint_updt_shadow_sig_info_map(ret, sig_info, word);
    pint_sig_info_needs_dump.push_back(ret);
    return ret;
}

void pint_reset_sig_init_value(ivl_nexus_t nex, sig_bit_val_t bit_updt) {
    pint_sig_info_t sig_info = pint_find_signal(nex);
    ivl_signal_t sig = sig_info->sig;
    unsigned updt_idx = pint_get_arr_idx_from_nex(nex);
    unsigned arr_words = sig_info->arr_words;
    unsigned width = sig_info->width;
    sig_info->value.clear();
    if (arr_words == 1) {
        pint_gen_siganl_init_value(sig_info->value, bit_updt, width);
    } else {
        vector<ivl_net_const_t> con;
        sig_bit_val_t bit_sig = pint_get_signal_type(sig);
        sig_bit_val_t bit_i;
        string buff = "{";
        string updt;
        string init;
        unsigned i;
        pint_gen_siganl_init_value(updt, bit_updt, width);
        pint_gen_siganl_init_value(init, bit_sig, width);
        for (i = 0; i < arr_words; i++) {
            if (i != updt_idx) {
                if (pint_signal_is_const(sig_info, i)) {
                    con = pint_parse_nex_con_info(ivl_signal_nex(sig, i));
                    pint_gen_const_value(buff, con);
                }
                if (bit_sig != BIT_Z) {
                    buff += init;
                } else {
                    bit_i = pint_get_sig_info_type(sig, i);
                    if (bit_i == BIT_Z) {
                        buff += init;
                    } else {
                        pint_gen_siganl_init_value(buff, bit_i, width);
                    }
                }
            } else {
                buff += updt;
            }
            buff += ", ";
        }
        buff.erase(buff.end() - 2, buff.end());
        buff += "}";
        sig_info->value = buff;
    }
}

//**    Dynamic lpm --API
ivl_nexus_t pint_find_nex(pint_sig_info_t sig_info, unsigned word) {
    ivl_signal_t sig = sig_info->sig;
    if (!sig) { //  force_shadow_signal may not have sig_info->sig
        return NULL;
    }
    if (word >= sig_info->arr_words) {
        return NULL;
    }
    return ivl_signal_nex(sig, word);
}

string pint_gen_signal_set_x_code(const char *sn, unsigned width) {
    if (width <= 4) {
        unsigned value = (1 << width) - 1;
        value |= value << 4;
        return string_format("\t%s = 0x%x;\n", sn, value);
    } else {
        string buff = "\t";
        unsigned cnt = (width + 0x1f) >> 5;
        unsigned i;
        for (i = 0; i < cnt - 1; i++) {
            buff += string_format("%s[%u] = 0xffffffff; %s[%u] = 0xffffffff; ", sn, i, sn, i + cnt);
        }
        i = cnt - 1;
        if (width & 0x1f) {
            unsigned value = (1 << (width & 0x1f)) - 1;
            buff +=
                string_format("%s[%u] = 0x%x; %s[%u] = 0x%x;\n", sn, i, value, sn, i + cnt, value);
        } else {
            buff +=
                string_format("%s[%u] = 0xffffffff; %s[%u] = 0xffffffff;\n", sn, i, sn, i + cnt);
        }
        return buff;
    }
}
