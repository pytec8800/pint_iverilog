#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "../../inc/cpu_adapt.h"
#include "../../inc/pint_simu.h"
#include "../../inc/pint_vpi.h"
#include "../common/sys_args.h"

#define pintCpyHostToDevice 0
#define pintCpyDeviceToHost 1

char print_str[1000000]; // size to be confirmed
int pint_fd_dump = 0;

enum PINT_READMEM_TYPE { PINT_READMEM_H = 0, PINT_READMEM_B };

int pint_atomic_add(volatile int *addr, int val) {
    int old_value = *addr;
    *addr = old_value + val;
    return old_value;
}

void pint_atomic_swap(volatile int *addr, int &val) {
    int old_value = *addr;
    *addr = val;
    val = old_value;
}

unsigned int pint_core_id() { return 0; }

unsigned int pint_core_num() { return 1; }

void pint_mcore_cycle_get(unsigned int *cycles) {
    struct timespec host_spec;
    unsigned long time;
    clock_gettime(CLOCK_REALTIME, &host_spec);
    time = host_spec.tv_sec * 1e+9 + host_spec.tv_nsec;

    cycles[0] = time & 0xFFFFFFFF;
    cycles[1] = (time >> 32) & 0xFFFFFFFF;
}

void pint_ncore_cycle_get(unsigned int *cycles) {
    // this func "pint_ncore_cycle_get" originally can only get the low 32bit
    unsigned int vv[2];
    pint_mcore_cycle_get(vv);
    cycles[0] = vv[0];
}

unsigned pint_enqueue_flag_set(unsigned int func_id) {
    unsigned int offset = func_id % 16;
    unsigned int words = func_id / 16;

    unsigned short val = pint_enqueue_flag[words];

    pint_enqueue_flag[words] = (val | (1 << offset));

    return (val & (1 << offset));
}

void pint_enqueue_flag_clear(unsigned int func_id) {
    unsigned int offset = func_id % 16;
    unsigned int words = func_id / 16;

    unsigned short val = pint_enqueue_flag[words];

    pint_enqueue_flag[words] = (val & (~(1 << offset)));
}

unsigned pint_enqueue_flag_set_any(unsigned short *buf, unsigned int func_id) {
    unsigned int offset = func_id % 16;
    unsigned int words = func_id / 16;

    unsigned short val = buf[words];

    buf[words] = (val | (1 << offset));

    return (val & (1 << offset));
}

void pint_enqueue_flag_clear_any(unsigned short *buf, unsigned int func_id) {
    unsigned int offset = func_id % 16;
    unsigned int words = func_id / 16;

    unsigned short val = buf[words];

    buf[words] = (val & (~(1 << offset)));
}

void printCHAR(unsigned int *addr, int c) { printf("%c", c); }

void printSTR(unsigned int *addr, char *s) { printf("%s", s); }

void printUINT(unsigned int *addr, unsigned int x) { printf("%u", x); }

extern int pint_sys_args_num;
extern int *pint_sys_args_off;
extern char *pint_sys_args_buff;

void host_malloc_sys_args() {
    pint_sys_args_off = malloc(SYS_ARGS_OFF_SIZE * sizeof(int));
    pint_sys_args_buff = malloc(SYS_ARGS_BUF_SIZE);
    if (pint_sys_args_off == NULL || pint_sys_args_buff == NULL) {
        Err_print("host_malloc_sys_args failed");
    }
    return;
}

void host_free_sys_args() {
    free(pint_sys_args_off);
    free(pint_sys_args_buff);
    return;
}

void parse_sys_args(int argc, char *argv[]) {
    int sys_args_num = 0;
    int sys_args_buf_len = 0;
    char *sys_args_buf = (char *)malloc(SYS_ARGS_BUF_SIZE);
    int *sys_args_off = (int *)malloc(SYS_ARGS_OFF_SIZE * sizeof(int));
    int valid_sys_arg_idx = 0;

    for (int i = 1; i < argc; i++) {
        char *arg_str = argv[i];
        if (arg_str[0] == '+') {
            int cur_str_len = strlen(arg_str + 1) + 1;
            if (sys_args_num >= SYS_ARGS_OFF_SIZE) {
                Err_print("Error! only support SYS_ARGS_OFF_SIZE sys args, %s will be lost!\n",
                          arg_str + 1);
                continue;
            }

            if (sys_args_buf_len + cur_str_len > SYS_ARGS_BUF_SIZE) {
                Err_print(
                    "Error! only support SYS_ARGS_BUF_SIZE bytes sys args, %s will be lost!\n",
                    arg_str + 1);
                continue;
            }

            sys_args_num++;
            strcpy(sys_args_buf + sys_args_buf_len, arg_str + 1);
            sys_args_off[valid_sys_arg_idx++] = sys_args_buf_len;
            sys_args_buf_len += cur_str_len;
        }
    }

    if (sys_args_num) {
        pint_sys_args_num = sys_args_num;
        memcpy((char *)pint_sys_args_off, (char *)sys_args_off, SYS_ARGS_OFF_SIZE * sizeof(int));
        memcpy((char *)pint_sys_args_buff, (char *)sys_args_buf, SYS_ARGS_BUF_SIZE);
    }

    free(sys_args_buf);
    free(sys_args_off);
}

/*-----------------------------/pint_driver/src/pintdrv/pint_vpi.c-----------------------------*/
int pint_vpi_fd[4096] = {0};
int pint_vpi_open_file_num = 0;

// vpi_fd 2 4 8 16 32 ...
int pint_vpifd_to_fd(unsigned vpi_fd) {
    int fd = 0;
    int idx = 0;
    unsigned vpi_fd_tmp = vpi_fd;

    // printf("vpi_fd:%d num:%d\n", vpi_fd, pint_vpi_open_file_num);
    if (vpi_fd <
        2) { //$fwrite and $fdisplay system tasks without using $fopen the fd is 1. 0 invald
        return vpi_fd;
    }
    if (vpi_fd > 0x80000000) {
        idx = 31;
        vpi_fd = vpi_fd - 0x80000000;
    }
    while (vpi_fd > 1) {
        vpi_fd = vpi_fd >> 1;
        idx++;
    }
    // printf("get:idx:%x vpi:%x fd:%x num:%x\n", idx, vpi_fd_tmp, pint_vpi_fd[idx],
    // pint_vpi_open_file_num);

    if (idx > pint_vpi_open_file_num) {
        printf("vpi_fd error: %x %x %x %x\n", vpi_fd_tmp, vpi_fd, idx, pint_vpi_open_file_num);
        // Err_print("vpi_fd error: %d %d\n", vpi_fd, idx);
        return -1;
    }
    return pint_vpi_fd[idx];
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 'a' - 'A';
    } else {
        return c;
    }
}

int htoi(char s[]) {
    int i;
    int n = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        i = 2;
    } else {
        i = 0;
    }
    for (; (s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z') ||
           (s[i] >= 'A' && s[i] <= 'Z');
         ++i) {
        if (tolower(s[i]) > '9') {
            n = 16 * n + (10 + tolower(s[i]) - 'a');
        } else {
            n = 16 * n + (tolower(s[i]) - '0');
        }
    }
    return n;
}

void pint_send_data_to_dev(unsigned dev_id, unsigned addr, void *data, unsigned byte,
                           unsigned dir) {
    if (pintCpyHostToDevice == dir) {
        memcpy((char *)addr, (char *)data, byte);
    } else {
        memcpy((char *)data, (char *)addr, byte);
    }
}

// type 0 h: 0-F  1 b:01xz_
bool pint_char_invaild(int type, char ch) {
    if (type == PINT_READMEM_H) {
        if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
            return 0;
        } else {
            return 1;
        }
    } else {
        if ((ch == '0' || ch == '1' || ch == 'x' || ch == 'z' || ch == '_')) {
            return 0;
        } else {
            return 1;
        }
    }
}

void pint_ch2sig_b(char *sig_data, char *str, unsigned wid) {
    if (wid <= 4) {
        char char_h = 0;
        char char_l = 0;
        for (int i = 0; i < wid; i++) {
            if (str[i] == 'x') {
                char_h += 1 << (wid - 1 - i);
                char_l += 1 << (wid - 1 - i);
            } else if (str[i] == 'z') {
                char_h += 1 << (wid - 1 - i);
            } else {
                char_l += (str[i] - '0') << (wid - 1 - i);
            }
        }
        *sig_data = (char_h << 4 | char_l);
    } else {
        unsigned *int_data = (unsigned *)sig_data;
        unsigned words = (wid + 31) / 32;
        unsigned rem = wid % 32;
        unsigned int_h = 0;
        unsigned int_l = 0;
        unsigned w = rem ? words - 1 : words;
        unsigned offset = wid - 1;

        for (int i = 0; i < w; i++) {
            int_h = 0;
            int_l = 0;
            for (int j = 0; j < 32; j++) {
                if (str[offset - j] == 'x') {
                    int_h += 1 << j;
                    int_l += 1 << j;
                } else if (str[offset - j] == 'z') {
                    int_h += 1 << j;
                } else {
                    int_l += (str[offset - j] - '0') << j;
                }
                // printf("j:%d %c[%x %x] ", j, );
            }
            int_data[i] = int_l;
            int_data[words + i] = int_h;
            offset -= 32;
        }

        if (rem) {
            int_h = 0;
            int_l = 0;
            for (int j = 0; j < rem; j++) {
                if (str[offset - j] == 'x') {
                    int_h += 1 << j;
                    int_l += 1 << j;
                } else if (str[offset - j] == 'z') {
                    int_h += 1 << j;
                } else {
                    int_l += (str[offset - j] - '0') << j;
                }
            }
            int_data[w] = int_l;
            int_data[words + w] = int_h;
        }
    }
}

void pint_msg_process(unsigned dev_id, unsigned char *data) {
    unsigned int msg_id, msg_type, msg_len;
    unsigned char func_name[1024] = {0};
    unsigned char file_name[1024] = {0};
    unsigned int sig_addr, start, len, sig_width, completed_flag;

    unsigned int *flag = malloc(4);
    *flag = 0x1;

    // PINT_SEND_MSG:1,0,0,readmemh,TEST1.ROM,33176,0,512,12,604
    // msg_id,type,len,func_name,file_name,sig_addr,start,len,sig_width,completed_flag
    sscanf(data, "%u,%u,%u,%s ,%s ,%u,%u,%u,%u,%u\n", &msg_id, &msg_type, &msg_len, func_name,
           file_name, &sig_addr, &start, &len, &sig_width, &completed_flag);
    // printf("process:%u,%u,%u,%s,%s,%u,%u,%u,%u,%u\n",  msg_id, msg_type, msg_len, func_name,
    // file_name, sig_addr, start, len, sig_width, completed_flag);

    if (!strcmp(func_name, "fopen")) {
        int fd;
        unsigned vpi_fd = 0;
        unsigned int *data_buff = malloc(8);
        unsigned open_type = start;
        // fd = open(file_name,O_RDWR | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR);
        fd = open(file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

        /*
         *open_type: 0:other 1:r 2:rb 3:w 4:wb
         *if type is omitted the file is opened for writung. (if write truncate to zero length or
         *create).
        */
        if ((open_type != 1) && (open_type != 2)) {
            ftruncate(fd, 0);
        }

        if (fd < 0) {
            Err_print("Error %d Unable to fopen %s\n", fd, file_name);
        }
        pint_vpi_open_file_num++; // $fwrite and $fdisplay system tasks without using $fopen the fd
                                  // is 1. 0 invald
        // 2 4 8 16 32 ... 0x80000000 +2 +4 +8  maximum of 63 files can be opened at the same time.

        pint_vpi_fd[pint_vpi_open_file_num] = fd;
        if (pint_vpi_open_file_num > 64) {
            Err_print(" open %s error. maximum of 63 files can be opened at the same time.\n",
                      file_name);
        }
        if (pint_vpi_open_file_num <= 32) {
            vpi_fd = 1 << pint_vpi_open_file_num;
        } else {
            vpi_fd = 0x80000000 + (1 << (pint_vpi_open_file_num - 31));
        }

        data_buff[0] = vpi_fd;
        pint_send_data_to_dev(dev_id, sig_addr, data_buff, 4, pintCpyHostToDevice);
        pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
        free(flag);
        free(data_buff);
        return;
    } else if (!strcmp(func_name, "fclose")) {
        int fd = pint_vpifd_to_fd(sig_addr);
        if (sig_addr < 0) {
            Err_print("Error Unable to fclose fd:%d\n", sig_addr);
            pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
            free(flag);
            return;
        }
        close(fd); // $fwrite and $fdisplay system tasks without using $fopen the fd is 1.

        pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
        free(flag);
        return;
    } else if (!strcmp(func_name, "fgetc")) {
        int fd = pint_vpifd_to_fd(sig_addr);
        unsigned int *data_buff = calloc(1, 8);
        if (fd < 0) {
            Err_print("Error Unable to fgetc fd:%u\n", fd);
            pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
            free(flag);
            return;
        }
        read(fd, data_buff, 1);
        data_buff[1] = 0;
        pint_send_data_to_dev(dev_id, start, data_buff, 8, pintCpyHostToDevice);
        pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
        free(flag);
        free(data_buff);
        return;
    } else if (!strcmp(func_name, "ungetc")) {
        // ch, fd, ret_addr
        unsigned fd = pint_vpifd_to_fd(sig_addr);
        unsigned ch = start;
        unsigned ret_addr = sig_width;
        unsigned int *data_buff = calloc(1, 8);
        if (fd < 0) {
            Err_print("Error Unable to ungetc fd:%u\n", fd);
            pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
            free(flag);
            return;
        }
        // offset - 1
        lseek(fd, -1, SEEK_CUR);
        write(fd, &ch, 1);
        lseek(fd, -1, SEEK_CUR);

        data_buff[0] = ch;
        data_buff[1] = 0;
        pint_send_data_to_dev(dev_id, ret_addr, data_buff, 8, pintCpyHostToDevice);
        pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
        free(flag);
        free(data_buff);
        return;
    } else if (!strcmp(func_name, "fgets")) {
        // code = $fgets(str, fd);
        int fd = pint_vpifd_to_fd(sig_addr);
        unsigned str = start;
        unsigned code = len;

        unsigned offset = lseek(fd, 0, SEEK_CUR);
        unsigned file_len = lseek(fd, 0, SEEK_END);
        unsigned char *str_buf = calloc(1, file_len + 1);
        unsigned char *strs = calloc(1, file_len + 1);
        int i = 0;
        unsigned j = 0;
        unsigned words = (sig_width + 31) / 32;
        unsigned int *data_buff = calloc(4, (words + 1) * 2);

        // printf("%d %d %d %d %d %d\n", fd, str, code, offset, file_len, sig_width);
        if (offset > file_len) {
            data_buff[0] = 0;
            pint_send_data_to_dev(dev_id, code, data_buff, 8, pintCpyHostToDevice);
            pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);

            free(data_buff);
            free(flag);
            return;
        }

        lseek(fd, offset, SEEK_SET);
        read(fd, str_buf, file_len - offset);
        while ((str_buf[i] != '\0') && (str_buf[i] != '\n') && (str_buf[i] != '\r')) {
            strs[i] = str_buf[i];
            i++;
        }
        strs[i] = str_buf[i];

        // printf("gets:%s words:%d, i:%d\n", strs, words, i);
        data_buff[0] = i;
        pint_send_data_to_dev(dev_id, code, data_buff, 8, pintCpyHostToDevice);
        data_buff[0] = 0;

        lseek(fd, offset + i + 1, SEEK_SET);
        for (; i >= 0; i--) {
            ((unsigned char *)data_buff)[j] = strs[i];
            j++;
        }
        pint_send_data_to_dev(dev_id, str, data_buff, words * 2 * 4, pintCpyHostToDevice);
        pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);

        free(str_buf);
        free(strs);
        free(data_buff);
        free(flag);
        return;
    } else if (!strcmp(func_name, "feof")) {
        int fd = pint_vpifd_to_fd(sig_addr);
        unsigned ret_addr = start;
        unsigned int *data_buff = calloc(4, 2);

        if (fd < 0) {
            // Err_print("Error Unable to feof fd:%d\n", fd);
            data_buff[0] = 0xffffffff; //-1
            pint_send_data_to_dev(dev_id, ret_addr, data_buff, 8, pintCpyHostToDevice);
            pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
            free(flag);
            return;
        }
        unsigned offset = lseek(fd, 0, SEEK_CUR);
        unsigned len = lseek(fd, 0, SEEK_END);

        // printf("len:%d cur:%d\n", len, offset);
        if (len < offset) {
            data_buff[0] = 1;
        } else {
            lseek(fd, offset, SEEK_SET);
            data_buff[0] = 0;
        }
        pint_send_data_to_dev(dev_id, ret_addr, data_buff, 8, pintCpyHostToDevice);
        pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
        free(data_buff);
        free(flag);
        return;
    } else if (!strcmp(func_name, "ftell") || !strcmp(func_name, "rewind")) {
        int fd = pint_vpifd_to_fd(sig_addr);
        unsigned ret_addr = start;
        unsigned int *data_buff = calloc(4, 2);
        unsigned offset;

        if (fd < 0) {
            // Err_print("Error Unable to ftell fd:%d\n", fd);
            data_buff[0] = 0xffffffff; //-1
            pint_send_data_to_dev(dev_id, ret_addr, data_buff, 8, pintCpyHostToDevice);
            pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
            free(flag);
            return;
        }

        if (!strcmp(func_name, "rewind")) {
            offset = lseek(fd, 0, SEEK_SET);
        } else {
            offset = lseek(fd, 0, SEEK_CUR);
        }
        data_buff[0] = offset;
        pint_send_data_to_dev(dev_id, ret_addr, data_buff, 8, pintCpyHostToDevice);
        pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
        free(data_buff);
        free(flag);
        return;
    } else if (!strcmp(func_name, "fseek")) {
        int fd = pint_vpifd_to_fd(sig_addr);
        int offset = start;
        int whence = len;
        unsigned ret_addr = sig_width;
        unsigned int *data_buff = calloc(4, 2);

        if (fd < 0) {
            // Err_print("Error Unable to fseek fd:%d\n", fd);
            data_buff[0] = 0xffffffff; //-1
            pint_send_data_to_dev(dev_id, ret_addr, data_buff, 8, pintCpyHostToDevice);
            pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
            free(flag);
            return;
        }
        // printf("%d %d %d %d\n", fd, offset, whence, ret_addr);
        if (whence == 0) {
            data_buff[0] = lseek(fd, offset, SEEK_SET);
        } else if (whence == 1) {
            data_buff[0] = lseek(fd, offset + lseek(fd, 0, SEEK_CUR), SEEK_SET);
        } else if (whence == 2) {
            data_buff[0] = lseek(fd, offset, SEEK_END);
        }

        // data_buff[0] = lseek(fd, offset, whence);
        data_buff[1] = 0;
        pint_send_data_to_dev(dev_id, ret_addr, data_buff, 8, pintCpyHostToDevice);
        pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
        free(data_buff);
        free(flag);
        return;
    } else if (!strcmp(func_name, "readmemh") || (!strcmp(func_name, "readmemb"))) {
        int fd = open(file_name, O_RDONLY);
        char *read_buff;
        int file_len = 0;
        int data_len = 0;
        int ch_per_idx = (sig_width + 3) / 4; // 0x12345678 b01xz100_
        int copy_words = 0;
        int read_type = PINT_READMEM_H; // 0:readmemh 1:readmemb
        unsigned send_len = 0;
        int sig_offset = 0;

        if (fd < 0) {
            Err_print("Error %d Unable to open %s\n", fd, file_name);
            pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
            return;
        }
        file_len = lseek(fd, 0, SEEK_END);
        read_buff = (char *)calloc(file_len + 1, 1);
        lseek(fd, 0, SEEK_SET);
        read(fd, read_buff, file_len);
        close(fd);

        unsigned char *src = read_buff;
        unsigned char *dst_buf =
            (char *)calloc((sig_width * len < file_len ? file_len : sig_width * len) + 16, 1);
        unsigned char *tmp_buf = (char *)calloc(sig_width + 1024, 1); // maybe len > sig_width
        unsigned section_num = 0;
        unsigned section_offset[1024];
        unsigned file_offset[1024];
        unsigned char *dst = dst_buf;

        if (!strcmp(func_name, "readmemb")) {
            read_type = PINT_READMEM_B;
            ch_per_idx = sig_width;
        }
        unsigned local_len = 0;
        // skip ' ' '_' '\n\t\r' and padding align to width
        while (*src != '\0') {
            if ((*src != ' ') && (*src != '\n') && (*src != '\r') && (*src != '\t')) {
                if (*src == '@') { //@000000010 offset addr
                    char str_offset[32];
                    int of = 0;
                    int offset = 0;
                    src++;
                    while ((*src != ' ') && (*src != '\n') && (*src != '\r') && (*src != '\t')) {
                        str_offset[of++] = *src;
                        src++;
                    }
                    str_offset[of] = '\0';
                    offset = htoi(str_offset);
                    section_offset[section_num] = offset;
                    file_offset[section_num] = data_len;
                    section_num++;
                    while ((*src != '\0') &&
                           ((*src == ' ') || (*src == '\n') || (*src == '\r') || (*src == '\t'))) {
                        src++;
                    }
                } else if (pint_char_invaild(read_type, *src)) {
                    if (*src == '/') {
                        if (*(src + 1) == '/') {
                            // there is annotation in the file
                            while (*src != '\n') {
                                src++;
                            }
                            src++;
                        } else if (*(src + 1) == '*') {
                            while (true) {
                                src++;
                                if (*src == '/' && *(src - 1) == '*')
                                    break;
                            }
                            src++;
                        } else {
                            printf("$%s(%s): Invalid input character: %c\n", func_name, file_name,
                                   *src);
                            src++;
                        }
                    } else {
                        printf("$%s(%s): Invalid input character: %c\n", func_name, file_name,
                               *src);
                        src++;
                    }
                } else if (*src == '_') {
                    // skip '_'
                    src++;
                } else {
                    tmp_buf[local_len] = *src;
                    local_len++;
                    src++;
                }
            } else {
                if (local_len == 0) {
                    src++;
                    continue;
                }
                tmp_buf[local_len] = 0;
                // printf("[%d %d %s]  ", ch_per_idx, local_len, tmp_buf);
                if (ch_per_idx < local_len) {
                    int offset = local_len - ch_per_idx;
                    for (int i = 0; i < ch_per_idx; i++) {
                        *dst = tmp_buf[offset + i];
                        dst++;
                    }
                } else {
                    if (ch_per_idx > local_len) {
                        int padlen = ch_per_idx - local_len;
                        for (int i = 0; i < padlen; i++) {
                            *dst = '0';
                            dst++;
                        }
                    }
                    for (int i = 0; i < local_len; i++) {
                        *dst = tmp_buf[i];
                        dst++;
                    }
                }

                data_len += ch_per_idx;
                local_len = 0;
                src++;
                while ((*src != '\0') &&
                       ((*src == ' ') || (*src == '\n') || (*src == '\r') || (*src == '\t'))) {
                    src++;
                }
            }
        }
        if (local_len) {
            if (ch_per_idx < local_len) {
                int offset = local_len - ch_per_idx;
                for (int i = 0; i < ch_per_idx; i++) {
                    *dst = tmp_buf[offset + i];
                    dst++;
                }
            } else {
                if (ch_per_idx > local_len) {
                    int padlen = ch_per_idx - local_len;
                    for (int i = 0; i < padlen; i++) {
                        *dst = '0';
                        dst++;
                    }
                }
                for (int i = 0; i < local_len; i++) {
                    *dst = tmp_buf[i];
                    dst++;
                }
            }
            data_len += ch_per_idx;
            local_len = 0;
        }

        *dst = '\0';
        if (section_num) {
            // printf("section_num:%d ", section_num);
            // for (int f = 0; f < section_num; f++)
            // {
            //     printf("[0x%x 0x%x]", section_offset[f], file_offset[f]);
            // }
            // printf("\n");
            copy_words = len;
        } else {
            copy_words = (data_len + ch_per_idx - 1) / ch_per_idx < len
                             ? (data_len + ch_per_idx - 1) / ch_per_idx
                             : len;
        }

        // printf("len:%d ch_per_idx:%d copy_words:0x%x\n dst:%s\n", data_len, ch_per_idx,
        // copy_words, dst_buf);
        char *sub_str = calloc(ch_per_idx + 4, 1);
        int w = (sig_width + 31) / 32;
        unsigned *sig_data = calloc(((copy_words * w * 4) * 2), 1);
        int offset = 0;
        int copy_ch_num;
        int section_idx = 0;
        int curr_offset = 0;
        char *char_sig_data = (char *)sig_data;

        if ((w == 1) || (read_type == PINT_READMEM_B)) { // width <= 32bit int
            copy_ch_num = ch_per_idx;
            for (int i = 0; i < copy_words; i++) {
                if (section_num) {
                    if (offset == file_offset[section_idx]) {
                        curr_offset = section_offset[section_idx];
                        // printf("section_idx:%d [0x%x %x]\n", section_idx, curr_offset, offset);
                        section_idx++;
                    }
                }

                if ((offset + ch_per_idx) > data_len) {
                    copy_ch_num = data_len - offset;
                }
                strncpy(sub_str, dst_buf + offset, copy_ch_num);
                if (read_type == PINT_READMEM_H) { // readmemh
                    if (ch_per_idx == 1) {         // width <= 4 char
                        sig_data[curr_offset++] = htoi(sub_str);
                    } else {
                        sig_data[(curr_offset)*2] = htoi(sub_str);
                        // sig_data[i*2+1] = 0;
                        curr_offset++;
                    }
                } else { // readmemb
                    if (sig_width <= 4) {
                        pint_ch2sig_b(&char_sig_data[curr_offset], sub_str, sig_width);
                    } else {
                        pint_ch2sig_b(&char_sig_data[curr_offset * 4 * w * 2], sub_str, sig_width);
                    }
                    curr_offset++;
                }

                offset += copy_ch_num;
                if ((curr_offset >= copy_words) || (copy_ch_num != ch_per_idx)) {
                    break;
                }
            }
        } else { // width > 32bit int* size of the end //readmemh
            copy_ch_num = ch_per_idx;
            unsigned num = copy_ch_num / 8;
            unsigned rem = copy_ch_num % 8;
            unsigned file_end = 0;
            unsigned copy_ch_len = 0;
            unsigned char *dst_buf_tmp = dst_buf;
            for (int i = 0; i < copy_words; i++) {
                if (section_num) {
                    if (offset == file_offset[section_idx]) {
                        curr_offset = section_offset[section_idx];
                        // printf("\nsection_idx:%d [0x%x %x]\n", section_idx, curr_offset, offset);
                        section_idx++;
                    }
                }
                unsigned n = 0;
                dst_buf_tmp = dst_buf_tmp + copy_ch_num;
                for (int j = 0; j < num; j++) {
                    if ((offset + 8) > data_len) {
                        file_end = 1;
                        copy_ch_len = data_len - offset;
                    } else {
                        copy_ch_len = 8;
                    }
                    n += copy_ch_len;
                    strncpy(sub_str, dst_buf_tmp - n, copy_ch_len);
                    sub_str[copy_ch_len] = 0;
                    sig_data[(curr_offset * 2 * w) + j] = htoi(sub_str);
                    offset += copy_ch_len;
                }
                if (rem && !file_end) {
                    if ((offset + rem) > data_len) {
                        file_end = 1;
                        copy_ch_len = data_len - offset;
                    } else {
                        copy_ch_len = rem;
                    }
                    n += copy_ch_len;

                    strncpy(sub_str, dst_buf_tmp - n, copy_ch_len);
                    sub_str[copy_ch_len] = 0;
                    sig_data[(curr_offset * 2 * w) + num] = htoi(sub_str);
                    offset += copy_ch_len;
                }
                curr_offset++;
                if ((curr_offset >= copy_words) || (file_end)) {
                    break;
                }
            }
        }
        // printf("sig_data:\n");
        // for(int i = 0; i < copy_words; i++) {
        //     if (sig_width > 4) {
        //         printf("[%02x %02x]", sig_data[2*i], sig_data[2*i+1]);
        //     } else {
        //         printf("%02x ", char_sig_data[i]);
        //     }
        // }
        // printf("\n");

        if (sig_width <= 4) {
            send_len = copy_words;
        } else {
            send_len = copy_words * w * 2 * 4;
        }
        if (start) {
            sig_offset = start * w * 2 * 4;
        }

        // printf("dev_id:%d sig_addr:%d send_len:%d\n", dev_id, sig_addr, send_len);
        pint_send_data_to_dev(dev_id, sig_addr + sig_offset, sig_data, send_len,
                              pintCpyHostToDevice);

        // printf("H2D data completed.\n");
        free(sub_str);
        free(sig_data);
        free(read_buff);
        free(dst_buf);
        free(tmp_buf);
    }
    pint_send_data_to_dev(dev_id, completed_flag, flag, 4, pintCpyHostToDevice);
    // printf("H2D flag completed. flag_addr:%d\n", completed_flag);
    free(flag);
}

void pint_simu_log(unsigned dev_id, char *log, unsigned len) {
    int type = 0;
    unsigned char *data;
    unsigned fd = 0;

    unsigned int data_offset = 0;
    unsigned char msg_type[32] = {0};
    for (int i = 0; i < len; i++) {
        msg_type[i] = log[i];
        data_offset++;
        if (log[i + 1] == ',') {
            data_offset++;
            break;
        }
        if (i == 31) {
            Err_print("msg type error\n");
            break;
        }
    }
    // printf("msg_type:%s data_offset:%u %s\n", msg_type, data_offset, log + data_offset);
    if (data_offset) {
        type = atoi(msg_type);
        data = log + data_offset;

        switch (type) {
        case PINT_PRINTF:
        case PINT_DISPLAY:
        case PINT_WRITE:
            printf("%s", data);
            break;
        case PINT_DUMP_FILE:
            sscanf(data, "%u,", &fd);
            pint_fd_dump = pint_vpifd_to_fd(fd);
            if (pint_fd_dump != -1) {
                // printf("%s file open succ.\n", data);
            } else {
                lseek(pint_fd_dump, 0, SEEK_SET);
            }
            break;
        case PINT_DUMP_VAR:
            write(pint_fd_dump, data, strlen(data));
            break;
        case PINT_FDISPLAY:
        case PINT_FWRITE:
            sscanf(data, "%u,", &fd); // data: fd,msg
            if (fd == 1) {
                printf("%s", &data[2]);
            } else {
                int i = 0;
                while (data[i]) {
                    if (data[i] == ',') {
                        break;
                    }
                    i++;
                }
                i++;
                if (fd % 2) {
                    printf("%s", &data[2]);
                    fd = fd - 1;
                }

                if (pint_vpifd_to_fd(fd) < 0) {
                    Err_print("%d vpifd error\n", type);
                } else {
                    write(pint_vpifd_to_fd(fd), &data[i], strlen(&data[i]));
                }
            }
            break;
        case PINT_SEND_MSG:
            // printf("PINT_SEND_MSG_00:%s\n", data);
            pint_msg_process(dev_id, data);
            break;

        case PINT_USER:
        default:
            Err_print("not found this kind of simu vpi:%d\n", type);
            break;
        }
    } else {
        Err_print("%s", log);
    }
}

void construct_printCHAR(int c) { sprintf(print_str, "%s%c", print_str, c); }

void construct_printUINT(unsigned int x) { sprintf(print_str, "%s%u", print_str, x); }

void construct_printSTR(char *s) { sprintf(print_str, "%s%s", print_str, s); }