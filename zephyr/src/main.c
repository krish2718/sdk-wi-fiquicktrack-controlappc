/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include "vendor_specific.h"
#include "eloop.h"
#include "indigo_api.h"
#include "utils.h"

#include <zephyr/posix/pthread.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wfa_qt, LOG_LEVEL_DBG);
int control_socket_init(int port);
void qt_main(void);
K_THREAD_DEFINE(qt_main_tid,
                CONFIG_WFA_QT_THREAD_STACK_SIZE,
                qt_main,
                NULL,
                NULL,
                NULL,
                0,
                0,
                0);

static pthread_t main_thread;
#define STACK_SIZE 4096
K_THREAD_STACK_DEFINE(main_thread_stack, STACK_SIZE);

void *main_thread_handler() {
    int service_socket = -1;

    /* Bind the service port and register to eloop */
    service_socket = control_socket_init(get_service_port());
    if (service_socket >= 0) {
        qt_eloop_run();
    } else {
        LOG_INF("Failed to initiate the UDP socket");
    }

    /* Stop eloop */
    qt_eloop_destroy();
    LOG_INF("ControlAppC stops");
    if (service_socket >= 0) {
        LOG_INF("Close service port: %d", get_service_port());
        close(service_socket);
    }

    return 0;
}

/* Show the welcome message with role and version */
static void print_welcome() {
    LOG_INF("Welcome to use QuickTrack Control App DUT version %s \n", TLV_VALUE_APP_VERSION);
}

void qt_main(void) {
    int ret =0;

    pthread_attr_t ptAttr;
    struct sched_param ptSchedParam;
    int ptPolicy;

    /* Welcome message */
    print_welcome();

    /* Print the run-time information */
    LOG_INF("QuickTrack control app running at: %d", get_service_port());
    LOG_INF("Wireless Interface: %s", WIRELESS_INTERFACE_DEFAULT);

    /* Register the callback */
    register_apis();

    /* Initiate the vendor's specific startup commands */
    vendor_init();

    /* Start eloop */
    qt_eloop_init(NULL);

    ret = pthread_attr_init(&ptAttr);
    if (ret != 0) {
        LOG_ERR("%s: pthread_attr_init failed: %d", __func__, ret);
        return;
    }

    ptSchedParam.sched_priority = 10;
    pthread_attr_setschedparam(&ptAttr, &ptSchedParam);
    pthread_attr_getschedpolicy(&ptAttr, &ptPolicy);
    pthread_attr_setschedpolicy(&ptAttr, SCHED_RR);
    pthread_attr_getschedpolicy(&ptAttr, &ptPolicy);

    ret = pthread_attr_setstack(&ptAttr, &main_thread_stack, 4096);
    if (ret != 0) {
        LOG_ERR("%s: pthread_attr_setstack failed: %d", __func__, ret);
        return;
    }

    ret = pthread_create(&main_thread, &ptAttr, main_thread_handler, NULL);
    if (ret < 0) {
        LOG_ERR("%s: pthread_create failed: %d", __func__, ret);
        return;
    }
}
