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
#include <poll.h>

#include "versions.h"
#include "common.h"
#include "signal_handling.h"
#include "chardev_operations.h"

#define TO_STR(x)                                   #x

typedef enum option_type
{
    OPT_TYPE_DEV_NUM = 0,
    OPT_TYPE_BIT_RATE = 1,
    OPT_TYPE_CYCLE_COUNT = 2,
    OPT_TYPE_BLOCKING_MODE = 3,
    OPT_TYPE_SEND_INTERVAL = 4,
    OPT_TYPE_DATA_FILE = 5,
    OPT_TYPE_POLL_TIMEOUT = 6,

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
    int32_t poll_timeout_msecs;
    char data_file[256];
    char cmd[8];
    uint64_t option_bits;
} cmdline_params_t;

#define DEFAULT_CYCLE_COUNT                         -1
#define DEFAULT_POLL_TIMEOUT                        10
#define DEFAULT_SEND_INTERVAL                       10000
#define DEFAULT_DATA_FILE                           "./pcanview.xmt"

static void show_help(const char *program, FILE *where)
{
    fprintf(where, "The application for testing Dual-interface PCAN-USB driver.\n");
    fprintf(where, "Usage: %s [command] [<option 1>[, <option 2>[, ...]]]\n", program);
    fprintf(where, "Supported commands:\n");
    fprintf(where, "    nop: No OPerations (for inner test only).\n");
    fprintf(where, "    read: Read and print data from device.\n");
    fprintf(where, "    write: Write data to device.\n");
    fprintf(where, "    get: Get value of the parameter specified -g option.\n");
    fprintf(where, "    set: Set the parameter to a value, both of which are specified by -s option.\n");
    fprintf(where, "Supported options:\n");
    fprintf(where, "    -b: Run in blocking mode.\n");
    fprintf(where, "    -c <cycle count>: Specify cycle count for test (%d if unspecified).\n", DEFAULT_CYCLE_COUNT);
    fprintf(where, "    -f <data file>: Specify data file (%s if unspecified).\n", DEFAULT_DATA_FILE);
    fprintf(where, "    -g <param_name>: Specify the parameter to get.\n");
    fprintf(where, "    -h: Show this help info.\n");
    fprintf(where, "    -i <send interval>: Specify send interval in microseconds (%d if unspecified).\n", DEFAULT_SEND_INTERVAL);
    fprintf(where, "    -n <device number>: Specify device number (%d if unspecified).\n", DEV_MINOR_BASE);
    fprintf(where, "    -r <bit rate>: Specify bit rate (%d if unspecified).\n", DEFAULT_BIT_RATE);
    fprintf(where, "    -s <pname>=<pvalue>: Specify the parameter and its value to set.\n");
    fprintf(where, "    -t <poll timeout>: Specify poll timeout in milliseconds (%d if unspecified).\n", DEFAULT_POLL_TIMEOUT);
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

    if (0 != strcmp("nop", cmd) &&
        0 != strcmp("read", cmd) && 0 != strcmp("write", cmd) &&
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
    cmdl_params->poll_timeout_msecs = DEFAULT_POLL_TIMEOUT;
    strcpy(cmdl_params->data_file, DEFAULT_DATA_FILE);

    while (-1 != (opt = getopt(argc, argv, "-bc:f:g:hi:n:r:s:t:v")))
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

        case 'g':
            break; /* TODO */

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

        case 's':
            break; /* TODO */

        case 't':
            SPECIFY_OPTION(cmdl_params->option_bits, OPT_TYPE_POLL_TIMEOUT);
            cmdl_params->poll_timeout_msecs = optarg ? atoi(optarg) : DEFAULT_POLL_TIMEOUT;
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

static int do_nop(int fd, const cmdline_params_t *cmdl_params)
{
    pause();

    return EXIT_SUCCESS;
}

//#define DYNAMIC_READ_BUFFER

static int do_read(int fd, const cmdline_params_t *cmdl_params)
{
#ifdef DYNAMIC_READ_BUFFER
    char *buf = (char *)calloc(PCAN_CHRDEV_MAX_BYTES_PER_READ * PCAN_CHRDEV_MAX_RX_BUF_COUNT + 1, sizeof(char));
#else
    char buf[PCAN_CHRDEV_MAX_BYTES_PER_READ * PCAN_CHRDEV_MAX_RX_BUF_COUNT + 1] = { 0 };
#endif
    ssize_t bytes;

    fprintf(stderr, "If there's no output, try this command: cat /dev/" DEV_NAME "%u\n", cmdl_params->dev_num);

    for (int32_t i = 0; ((cmdl_params->cycle_count < 0) ? true : (i < cmdl_params->cycle_count)); ++i)
    {
        if (sig_check_critical_flag())
        {
            fprintf(stderr, "Interrupted by signal.\n");
            break;
        }

        if (!cmdl_params->is_blocking)
        {
            struct pollfd pfd;
            int ret;

            pfd.fd = fd;
            pfd.events = POLLIN;

            ret = poll(&pfd, 1, cmdl_params->poll_timeout_msecs);

            if (0 == ret) /* Timed out. */
                continue;

            if (ret < 0)
            {
                perror("poll failure");
                break;
            }

            if (0 != (pfd.revents & POLLERR))
            {
                fprintf(stderr, "Error occurred during polling, see kernel log for more details.\n");
                break;
            }

            if (0 != (pfd.revents & POLLHUP))
            {
                fprintf(stderr, "Hang up!\n");
                break;
            }

            if (0 != (pfd.revents & POLLNVAL))
            {
                fprintf(stderr, "Invalid polling request!\n");
                break;
            }
        }

        bytes = read(fd, buf, PCAN_CHRDEV_MAX_BYTES_PER_READ * PCAN_CHRDEV_MAX_RX_BUF_COUNT + 1);

        if (bytes > 0)
        {
            printf("%s", buf);
            continue;
        }

        if (bytes < 0)
            perror("Read exception (but not always failure)");

        break;
    }

#ifdef DYNAMIC_READ_BUFFER
    free(buf);
#endif

    return EXIT_SUCCESS;
}

static int do_write(int fd, const cmdline_params_t *cmdl_params)
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
    int oflags = ((0 == strcmp("write", cmd) || 0 == strcmp("set", cmd)) ? O_RDWR : O_RDONLY)
        | (cmdl_params->is_blocking ? 0 : O_NONBLOCK);
    int fd = 0;

    snprintf(dev_path, sizeof(dev_path) - 1, "/dev/" DEV_NAME "%u", cmdl_params->dev_num);

    if ((fd = open(dev_path, oflags)) < 0)
    {
        perror(dev_path);
        return EXIT_FAILURE;
    }

    int ret = EXIT_SUCCESS;

    if (0 == strcmp("nop", cmd))
        ret = do_nop(fd, cmdl_params);
    else if (0 == strcmp("read", cmd))
        ret = do_read(fd, cmdl_params);
    else if (0 == strcmp("write", cmd))
        ret = do_write(fd, cmdl_params);
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
    int err = 0;

    parse_command_line(argc, argv, &cmdl_params);

    if ((err = sig_simple_register()) < 0)
        fprintf(stderr, "sig_simple_register() failed: %s\n", sig_error(err));

    return err ? EXIT_FAILURE : handle_command(&cmdl_params);
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
 *
 * >>> 2023-12-28, Man Hung-Coeng <udc577@126.com>:
 *  01. Remove recv and send commands,
 *      and add nop, read (implemented) and write commands.
 */

