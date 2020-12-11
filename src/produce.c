/**
 * @filename   produce.c
 *  A sample shows how to produce messages into kafka with kafkatools api.
 *
 * @author     Liang Zhang <350137278@qq.com>
 * @version    0.1.0
 * @create     2017-12-20
 * @update     2020-05-22 12:32:50
 */
#include "kafkatools.h"

#include <common/misc.h>

#define APPNAME    "produce"

#define PROPSFILE_LEN_MAX  255

#define MESSAGE_SIZE    1024

#define MESSAGES_MAX    10000

#define MESSAGE_TOPIC  "test:0-7"

#define GET_PARTITIONID(id, minid, maxid)  \
    ((id) % (maxid - minid + 1) + minid)

ub8 message_uid = 0;

// kafka state report callback
//   do not send message in this callback
//
static void producer_notify_state (rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *statep)
{
    ktproducer_state_t *state = (ktproducer_state_t *)statep;

    message_uid++;

    if (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        printf("(produce.c:%d) delivery failed: %s.\n", __LINE__, rd_kafka_err2str(rkmessage->err));
    } else {
        if (message_uid % 10000 == 0) {
            if (rkmessage->key_len) {
            #ifdef PRINT_MESSAGE_PAYLOAD
                printf("(produce.c:%d) success(%" PRIu64"): topic='%s:%d' key=<%.*s> msg={%.*s}\n",
                    __LINE__, message_uid,
                    rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition,
                    (int)rkmessage->key_len, (char *)rkmessage->key,
                    (int)rkmessage->len, (char *)rkmessage->payload);
            #else
                printf("(produce.c:%d) success(%" PRIu64"): topic='%s:%d' key=<%.*s> msg={%d bytes}\n",
                    __LINE__, message_uid,
                    rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition,
                    (int)rkmessage->key_len, (char *)rkmessage->key, (int)rkmessage->len);
            #endif
            } else {
            #ifdef PRINT_MESSAGE_PAYLOAD
                printf("(produce.c:%d) success(%" PRIu64"): topic='%s:%d' msg={%.*s}\n",
                    __LINE__, message_uid,
                    rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition,
                    (int)rkmessage->len, (char *)rkmessage->payload);
            #else
                printf("(produce.c:%d) success(%" PRIu64"): topic='%s:%d' msg={%d bytes}\n",
                    __LINE__, message_uid,
                    rd_kafka_topic_name(rkmessage->rkt), rkmessage->partition, (int)rkmessage->len);
            #endif
            }
        }
    }
}


/**************************************************************
 * Usage:
 *
 *   %prog topic /path/to/propertiesfile messages
 *************************************************************/

int main (int argc, char *argv[])
{
    WINDOWS_CRTDBG_ON

    int i, ret;

    struct timespec t1, t2;

    ktproducer_state_t state = {0};

    WINDOWS_CRTDBG_ON

    int messages = MESSAGES_MAX;

    if (argc == 2) {
        printf("set topic={%s}\n", argv[1]);

        ret = kafkatools_producer_state_init(NULL, argv[1], producer_notify_state, NULL, &state);
    } else if (argc == 3) {
        printf("set topic={%s} properties='%s'\n", argv[1], argv[2]);

        ret = kafkatools_producer_state_init(argv[2], argv[1], producer_notify_state, NULL, &state);
    } else if (argc == 4) {
        messages = (int) atoi(argv[3]);

        printf("set topic={%s} properties='%s' messages=%d\n", argv[1], argv[2], messages);

        ret = kafkatools_producer_state_init(argv[2], argv[1], producer_notify_state, NULL, &state);
    } else {
        printf("Usage:\n    %s topic </path/to/propertiesfile> <messages>\n", APPNAME);
        exit(0);
    }

    //   kafkatools_producer_state_init(NULL, "test:0-7", notify_state_cb, NULL, &state);
    ret = kafkatools_producer_state_init((argc == 3? argv[2] : 0), (argc > 1? argv[1] : MESSAGE_TOPIC), producer_notify_state, NULL, &state);
    if (ret != KAFKATOOLS_SUCCESS) {
        exit(EXIT_FAILURE);
    }

    // You can use this "state" in your message produce thread like below:
    do {
        void * thrarg = (void*) &state;
        int err;

        ktproducer_state_t * thrstate = (ktproducer_state_t *) thrarg;

        char keybuf[32];
        char payload[MESSAGE_SIZE];

        kafkatools_msg_data_t  msg = {0};

        for (i = 0; i < MESSAGE_SIZE; i++) {
            payload[i] = (char)GET_PARTITIONID(i, 0x20, 0x7E);
        }
        payload[MESSAGE_SIZE - 1] = 0;

        getnowtimeofday(&t1);
        
        for (i = 0; i < messages; i++) {
            msg.key = keybuf;
            msg.keylen = snprintf(keybuf, sizeof(keybuf), "key_%d", i);

            msg.msgbuf = payload;
            msg.msglen = MESSAGE_SIZE - 1;

            // set partition between [partitionid_min, partitionid_max]
            // use random or your biz policy to determine which partition you want!
            thrstate->site.partition = GET_PARTITIONID(i, thrstate->site.partitionid_min, thrstate->site.partitionid_max);

            err = kafkatools_produce_timedwait(thrstate->producer, &thrstate->site, &msg, 1000, 10);
            if (err != KAFKATOOLS_SUCCESS) {
                const char * errstr = kafkatools_producer_get_errstr(thrstate->producer, &err);
                printf("(produce.c:%d) kafkatools_produce_timedwait error: %s\n", __LINE__, errstr);
            }
        }

        getnowtimeofday(&t2);
    } while(0);

    kafkatools_producer_state_uninit(&state, KAFKATOOLS_WAIT_INFINITE);

    double elapsed_ms = (double) difftime_msec(&t1, &t2);
    if (elapsed_ms < 1) {
        elapsed_ms = 1;
    }

    printf("(produce.c:%d) total %d messages success. elapsed %.0lf ms. speed = %d/S.\n",
        __LINE__, messages, elapsed_ms, (int)(messages / (elapsed_ms / 1000)));

    return 0;
}
