// SPDX-License-Identifier: GPL-2.0

/*
 * The application for setting and testing Dual-interface PCAN-USB driver.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "versions.h"
#include "common.h"

#define TO_STR(x)                                   #x

typedef enum option_type
{
    OPT_TYPE_DEV_NUM = 0,
    OPT_TYPE_BIT_RATE = 1,
    OPT_TYPE_CYCLE_COUNT = 2,
    OPT_TYPE_BLOCKING_MODE = 3,
    OPT_TYPE_SEND_INTERVAL = 4,
    OPT_TYPE_DATA_FILE = 5,

    OPT_TYPE_MAX
} option_type_t;

#define SPECIFY_OPTION(opt_bits, opt_type)          ((opt_bits) |= (((uint64_t)1) << (opt_type)))
#define OPTION_IS_SPECIFIED(opt_bits, opt_type)     ((opt_bits) & (((uint64_t)1) << (opt_type)))

typedef struct cmdline_params
{
    uint32_t dev_num;
    uint32_t bit_rate;
    int32_t cycle_count;
    uint32_t is_blocking:1;
    uint32_t send_interval_usecs:31;
    char data_file[256];
    char cmd[8];
    uint64_t option_bits;
} cmdline_params_t;

#define DEFAULT_CYCLE_COUNT                         -1
#define DEFAULT_SEND_INTERVAL                       10000
#define DEFAULT_DATA_FILE                           "./pcanview.xmt"

static void show_help(const char *program, FILE *where)
{
    fprintf(where, "The application for setting and testing Dual-interface PCAN-USB driver.\n");
    fprintf(where, "Usage: %s [command] [<option 1>[, <option 2>[, ...]]]\n", program);
    fprintf(where, "Supported commands:\n");
    fprintf(where, "    recv: Receive and print data from device.\n");
    fprintf(where, "    send: Send data to device.\n");
    fprintf(where, "    get: Get value of the parameter specified by option after this command.\n");
    fprintf(where, "    set: Set the parameter to a value, both of which are specified by option after this command.\n");
    fprintf(where, "Supported options:\n");
    fprintf(where, "    -b: Run in blocking mode.\n");
    fprintf(where, "    -c[cycle count]: Specify cycle count for test (%d if unspecified).\n", DEFAULT_CYCLE_COUNT);
    fprintf(where, "    -f[data file]: Specify data file (%s if unspecified).\n", DEFAULT_DATA_FILE);
    fprintf(where, "    -h: Show this help info.\n");
    fprintf(where, "    -i[send interval]: Specify send interval in microseconds (%d if unspecified).\n", DEFAULT_SEND_INTERVAL);
    fprintf(where, "    -n[device number]: Specify device number (%d if unspecified).\n", DEV_MINOR_BASE);
    fprintf(where, "    -r[bit rate]: Specify bit rate (%d if unspecified).\n", DEFAULT_BIT_RATE);
    fprintf(where, "    -v: Show version.\n");
}

static void parse_command_line(int argc, char **argv, cmdline_params_t *cmdl_params)
{
    if (argc < 2)
    {
        show_help(argv[0], stderr);
        exit(EXIT_FAILURE);
    }

    memset(cmdl_params, 0, sizeof(*cmdl_params));

    char *cmd = cmdl_params->cmd;

    strncpy(cmd, argv[1], sizeof(cmdl_params->cmd) - 1);
    cmd[sizeof(cmdl_params->cmd) - 1] = '\0';

    if (0 != strcmp("recv", cmd) && 0 != strcmp("send", cmd) &&
        0 != strcmp("get", cmd) && 0 != strcmp("set", cmd) &&
        0 != strcmp("-h", cmd) && 0 != strcmp("-v", cmd))
    {
        fprintf(stderr, "*** Invalid command: %s\n", cmd);
        fprintf(stderr, "Run with \"-h\" option for help.\n");
        exit(EXIT_FAILURE);
    }

    if (0 != strcmp("-h", cmd) && 0 != strcmp("-v", cmd))
    {
        --argc;
        ++argv;
    }

    int opt = 0;

    cmdl_params->dev_num = DEV_MINOR_BASE;
    cmdl_params->bit_rate = DEFAULT_BIT_RATE;
    cmdl_params->cycle_count = DEFAULT_CYCLE_COUNT;
    cmdl_params->send_interval_usecs = DEFAULT_SEND_INTERVAL;
    strcpy(cmdl_params->data_file, DEFAULT_DATA_FILE);

    while (-1 != (opt = getopt(argc, argv, "-bc::f::hi::n::r::v")))
    {
        switch (opt)
        {
        case 'b':
            cmdl_params->is_blocking = true;
            break;

        case 'c':
            cmdl_params->cycle_count = optarg ? atoi(optarg) : DEFAULT_CYCLE_COUNT;
            break;

        case 'f':
            memset(cmdl_params->data_file, 0, sizeof(cmdl_params->data_file));
            if (optarg)
                strncpy(cmdl_params->data_file, optarg, sizeof(cmdl_params->data_file) - 1);
            else
                strcpy(cmdl_params->data_file, DEFAULT_DATA_FILE);
            break;

        case 'h':
            show_help(argv[0], stdout);
            exit(EXIT_SUCCESS);

        case 'i':
            cmdl_params->send_interval_usecs = optarg ? atoi(optarg) : DEFAULT_SEND_INTERVAL;
            break;

        case 'n':
            cmdl_params->dev_num = optarg ? atoi(optarg) : DEV_MINOR_BASE;
            break;

        case 'r':
            SPECIFY_OPTION(cmdl_params->option_bits, OPT_TYPE_BIT_RATE);
            cmdl_params->bit_rate = optarg ? atoi(optarg) : DEFAULT_BIT_RATE;
            break;

        case 'v':
            fprintf(stdout, APP_VERSION "-" __VER__ "\n");
            exit(EXIT_SUCCESS);

        case '?':
            fprintf(stderr, "Unknown option -%c\n", opt);
            exit(EXIT_FAILURE);

        default:
            fprintf(stderr, "Found an orphan argument: %s\n", optarg);
            continue;
        }
    } /* while (getopt()) */

    if (0 == cmdl_params->option_bits && (0 == strcmp("get", cmd) || 0 == strcmp("set", cmd)))
    {
        fprintf(stderr, "*** You've not specified what to %s!\n", cmd);
        fprintf(stderr, "Run with \"-h\" option for help.\n");
        exit(EXIT_FAILURE);
    }
}

static bool s_should_exit = false;

static void set_exit_flag(int signum)
{
    s_should_exit = true;
}

static inline bool should_exit(void)
{
    return s_should_exit;
}

static void register_signals(void)
{
    /*
     * FIXME: sigaction()
     */
    signal(SIGINT, set_exit_flag);
    signal(SIGQUIT, set_exit_flag);
    signal(SIGABRT, set_exit_flag);
    signal(SIGTERM, set_exit_flag);
}

static int do_recv(int fd, const cmdline_params_t *cmdl_params)
{
    for (int32_t i = 0; ((cmdl_params->cycle_count < 0) ? true : (i < cmdl_params->cycle_count)); ++i)
    {
        if (should_exit())
        {
            fprintf(stderr, "Interrupted by signal.\n");
            break;
        }

        sleep(1); /* TODO */
    }

    return EXIT_SUCCESS;
}

static int do_send(int fd, const cmdline_params_t *cmdl_params)
{
    printf("%s: TODO ...\n", cmdl_params->cmd);

    return EXIT_SUCCESS;
}

static int do_get(int fd, const cmdline_params_t *cmdl_params)
{
    printf("%s: TODO ...\n", cmdl_params->cmd);

    return EXIT_SUCCESS;
}

static int do_set(int fd, const cmdline_params_t *cmdl_params)
{
    printf("%s: TODO ...\n", cmdl_params->cmd);

    return EXIT_SUCCESS;
}

static int handle_command(const cmdline_params_t *cmdl_params)
{
    char dev_path[32] = { 0 };
    const char *cmd = cmdl_params->cmd;
    int oflags = ((0 == strcmp("send", cmd) || 0 == strcmp("set", cmd)) ? O_RDWR : O_RDONLY)
        | (cmdl_params->is_blocking ? 0 : O_NONBLOCK);
    int fd = 0;

    snprintf(dev_path, sizeof(dev_path) - 1, "/dev/" DEV_NAME "%u", cmdl_params->dev_num);

    if ((fd = open(dev_path, oflags)) < 0)
    {
        perror(dev_path);
        return EXIT_FAILURE;
    }

    int ret = EXIT_SUCCESS;

    if (0 == strcmp("recv", cmd))
        ret = do_recv(fd, cmdl_params);
    else if (0 == strcmp("send", cmd))
        ret = do_send(fd, cmdl_params);
    else if (0 == strcmp("get", cmd))
        ret = do_get(fd, cmdl_params);
    else
        ret = do_set(fd, cmdl_params);

    close(fd);

    return ret;
}

int main(int argc, char **argv)
{
    cmdline_params_t cmdl_params = { 0 };

    parse_command_line(argc, argv, &cmdl_params);

    register_signals();

    return handle_command(&cmdl_params);
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-07-19, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Change license to GPL-2.0.
 *
 * >>> 2023-11-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Build up the skeleton.
 */

