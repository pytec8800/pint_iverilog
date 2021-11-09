#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/file.h>
#include <time.h>
#include <fcntl.h>
#include <pwd.h>

#include "pint_runtime_api.h"
#include "../common/sys_args.h"

#define SYS_ARGS_BUF_SIZE 400
#define SYS_ARGS_OFF_SIZE 16

#define CheckPintApi(expr)                                                                         \
    do {                                                                                           \
        pintError_t errcode = expr;                                                                \
        if (errcode != pintSuccess) {                                                              \
            fprintf(stderr, "\n[pintapi error] %s"                                                 \
                            "\nlocation: %s:%d"                                                    \
                            "\nerror code: %d"                                                     \
                            "\n",                                                                  \
                    #expr, __FILE__, __LINE__, (int)errcode);                                      \
            exit(-1);                                                                              \
        }                                                                                          \
    } while (false)

enum oper { begin, end };

int RunTest(char *path, pintArgs_t *kernel_args, int args_num, int dev_id) {
    pintError_t err = pintSuccess;

#ifdef SIMU_RECORD
    record(begin, dev_id);
#endif

    char *file_path[] = {path};
    pintProgram_t pint_program;

    assert(pintSuccess == pintCreateProgram(&pint_program, file_path, 1, "", pintBinaryFile));
    assert(pintSuccess == pintLaunchProgram(pint_program, kernel_args, args_num, NULL, 0));
    assert(pintSuccess == pintDeviceSynchronize());
    pintFreeProgram(pint_program);
#ifdef SIMU_RECORD
    record(end, dev_id);
#endif

    return 0;
}

void argsMemcpyToDevice(int **sys_args_off, char **sys_args_buf, unsigned int **d_sys_args_off,
                        unsigned int **d_sys_args_buf) {
    int sizeof_sys_args_off = SYS_ARGS_OFF_SIZE * sizeof(int);
    int sizeof_sys_args_buf = SYS_ARGS_BUF_SIZE;
    CheckPintApi(pintMalloc((void **)d_sys_args_off, sizeof_sys_args_off));
    CheckPintApi(pintMemcpy(*d_sys_args_off, 0, *sys_args_off, 0, sizeof_sys_args_off,
                            pintMemcpyHostToDevice));

    CheckPintApi(pintMalloc((void **)d_sys_args_buf, sizeof_sys_args_buf));
    CheckPintApi(pintMemcpy(*d_sys_args_buf, 0, *sys_args_buf, 0, sizeof_sys_args_buf,
                            pintMemcpyHostToDevice));
    return;
}

int record(enum oper flag, int dev_id) {
    time_t now;
    struct tm *tm_now;
    time(&now);
    tm_now = localtime(&now);
    char log_file[64];
    sprintf(log_file, "/tmp/pint_record_%d%d%d.log", tm_now->tm_year + 1900, tm_now->tm_mon + 1,
            tm_now->tm_mday);
    char record_str[128];
    if (access(log_file, R_OK) != 0) {
        umask(0);
        if (creat(log_file, 0777) < 0) {
            printf("create log file %s fail!\n", log_file);
            return -1;
        }
    }
    struct passwd *pwd;
    pid_t pid = getpid();
    pwd = getpwuid(getuid());
    if (flag == begin) {
        sprintf(record_str, "user:%s\tpid:%d\tdev_id:%d\tbegin_time:%d-%d-%d\n", pwd->pw_name, pid,
                dev_id, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    } else {
        sprintf(record_str, "user:%s\tpid:%d\tdev_id:%d\tend_time:%d-%d-%d\n", pwd->pw_name, pid,
                dev_id, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    }
    FILE *fp;
    if ((fp = fopen(log_file, "aw+")) == NULL) {
        printf("open log file %s fail!\n", log_file);
    }
    if (0 == flock(fileno(fp), LOCK_EX)) {
        fseek(fp, 0, SEEK_END);
        fwrite(record_str, strlen(record_str), 1, fp);
        fclose(fp);
        flock(fileno(fp), LOCK_UN);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    struct timeval time_begin, time_end;
    double exe_time, b_time, e_time;
    char *path;
    int status = 0;
    int i;
    gettimeofday(&time_begin, NULL);

    path = "pint.pin";

    int sys_args_num = 0;
    int sys_args_buf_len = 0;
    char *sys_args_buf = (char *)malloc(SYS_ARGS_BUF_SIZE);
    int *sys_args_off = (int *)malloc(SYS_ARGS_OFF_SIZE * sizeof(int));
    int valid_sys_arg_idx = 0;
    for (i = 1; i < argc; i++) {
        char *arg_str = argv[i];
        if (arg_str[0] == '+') {
            int cur_str_len = strlen(arg_str + 1) + 1;
            if (sys_args_num >= SYS_ARGS_OFF_SIZE) {
                printf("Error! only support SYS_ARGS_OFF_SIZE sys args, %s will be lost!\n",
                       arg_str + 1);
                continue;
            }

            if (sys_args_buf_len + cur_str_len > SYS_ARGS_BUF_SIZE) {
                printf("Error! only support SYS_ARGS_BUF_SIZE bytes sys args, %s will be lost!\n",
                       arg_str + 1);
                continue;
            }

            sys_args_num++;
            strcpy(sys_args_buf + sys_args_buf_len, arg_str + 1);
            sys_args_off[valid_sys_arg_idx++] = sys_args_buf_len;
            sys_args_buf_len += cur_str_len;
        }
    }

    int dev_id = -1;
    pintGetOneExclusiveDevice(&dev_id);
    printf("Get one avaliable device id: %d\n", dev_id);

    unsigned int *d_sys_args_off = NULL;
    unsigned int *d_sys_args_buf = NULL;
    if (sys_args_num > 0) {
        argsMemcpyToDevice(&sys_args_off, &sys_args_buf, &d_sys_args_off, &d_sys_args_buf);
        pintArgs_t kernel_args[] = {
            {"pint_sys_args_num", &sys_args_num},
            {"pint_sys_args_off", d_sys_args_off},
            {"pint_sys_args_buff", d_sys_args_buf},
        };
        printf("RunTest finished.\n");
        status = RunTest(path, kernel_args, 3, dev_id);
        printf("RunTest finished.\n");

    } else {
        status = RunTest(path, NULL, 0, dev_id);
    }

    free(sys_args_buf);
    free(sys_args_off);
    if (sys_args_num > 0) {
        CheckPintApi(pintFree(d_sys_args_off));
        CheckPintApi(pintFree(d_sys_args_buf));
    }
    gettimeofday(&time_end, NULL);
    e_time = time_end.tv_sec + (time_end.tv_usec * 1.0) / 1000000.0;
    b_time = time_begin.tv_sec + (time_begin.tv_usec * 1.0) / 1000000.0;

    if (e_time >= b_time) {
        exe_time = e_time - b_time;
    } else {
        exe_time = (0xffffffffffffffff - b_time) + e_time;
    }

    usleep(200000);
    printf("Device execute: %f seconds.\n", exe_time);
    if (status) {
        printf("Dev execute FAILED!!\n");
    }
    return 0;
}
