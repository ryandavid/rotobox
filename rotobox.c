#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rotobox.h"
#include "dump978.h"

bool exitRequested = false;

void handle_sigint() {
    dump978_worker_stop();
    exitRequested = true;
}

int main(int argc, char **argv) {
    pthread_t thread_978;

    signal(SIGINT, handle_sigint);

    if (dump978_init("0978") == false) {
        fprintf(stdout, "ERROR: Could not init dump978\n");

        return 0;
    }

    int success = pthread_create(&thread_978, NULL, dump978_worker, NULL);
    if (success != 0) {
        fprintf(stdout, "ERROR: Could not create reader thread (Return = %d)\n", success);
    }

    while (exitRequested == false) {
        struct uat_mdb mdb;

        if (dump978_get_mdb(&mdb) == true) {
            fprintf(stdout, "\nts=%llu, rs=%d\n", mdb.timestamp, mdb.receiveErrors);

            if (mdb.msgType == UAT_MDB_TYPE_ADSB) {
                uat_display_adsb_mdb(&mdb.u.adsb_mdb, stdout);
            } else {
                uat_display_uplink_mdb(&mdb.u.uplink_mdb, stdout);
            }
            fflush(stdout);
        }
    }

    pthread_join(thread_978, NULL);

    fprintf(stdout, "Exiting!\n");
    dump978_cleanup();

    return 0;
}

