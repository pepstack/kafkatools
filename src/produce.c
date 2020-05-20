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

#define PROPSFILE_LEN_MAX  255


#define MYTOPIC  "test"

// kafka state report callback
//   do not send message in this callback
//
static void produce_state_cb (rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *state)
{
    ub8 id = (ub8) (rkmessage->_private);

    if (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        printf("[%ju] message delivery failed: %s.\n", id, rd_kafka_err2str(rkmessage->err));
    } else {
        if (rkmessage->key_len) {
            printf("produce success on (topic=%s partition=%d %zd bytes): msg={%.*s} key=<%.*s>\n",
                rd_kafka_topic_name(rkmessage->rkt),
                rkmessage->partition,
                rkmessage->len,
                (int) rkmessage->len, (char *) rkmessage->payload,
                (int) rkmessage->key_len, (char *) rkmessage->key);
        } else {
            printf("produce success on (topic=%s partition=%d %zd bytes): {%.*s}\n",
                rd_kafka_topic_name(rkmessage->rkt),
                rkmessage->partition,
                rkmessage->len,
                (int) rkmessage->len, (char *) rkmessage->payload);
        }
    }
}


/**************************************************************
 * Usage:
 *
 *   %prog propertiesfile
 *************************************************************/

int main (int argc, char *argv[])
{
    int ret;
    ktproducer_state_t state = {0};

    WINDOWS_CRTDBG_ON

    ret = kafkatools_producer_state_init((argc == 3? argv[2] : 0), (argc > 1? argv[1] : MYTOPIC), produce_state_cb, 0, &state);
    if (ret != KAFKATOOLS_SUCCESS) {
        exit(EXIT_FAILURE);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // You can use this "state" in your message produce thread like below:
    do {
        void * thrarg = (void*) &state;
        int err;

        ktproducer_state_t * thrstate = (ktproducer_state_t *) thrarg;

        char keybuf[256];
        char payload[256];

        kafkatools_msg_data_t  msg = {0};

        msg.msgbuf = payload;
        msg.key = keybuf;

        msg.keylen = snprintf(keybuf, sizeof(keybuf), "key_%d", 0);
        msg.msglen = snprintf(payload, sizeof(payload), "msg_%d", 0);

        // set partition between [partitionid_min, partitionid_max]
        // use random or your biz policy to determine which partition you want!
        thrstate->site.partition = 0;

        err = kafkatools_produce_timedwait(thrstate->producer, &thrstate->site, &msg, 1000, 10);
        if (err != KAFKATOOLS_SUCCESS) {
            const char * errstr = kafkatools_producer_get_errstr(thrstate->producer, &err);
            // LOGGER_ERROR(...);
        }
    } while(0);
    ////////////////////////////////////////////////////////////////////////////////

    kafkatools_producer_state_uninit(&state);
    return 0;
}

///////////////////////////////////////////////////////////////
// DELETE
static void produce_messages (kt_producer producer, kafkatools_msg_site_t *site, ub8 startid, ub8 maxcount)
{
    int ret;

    ub8 id;

    char keybuf[256];
    char payload[256];

    kafkatools_msg_data_t  msg;

    msg.msgbuf = payload;
    msg.key = keybuf;

    id = startid;
    maxcount += startid;

    while (id <= maxcount) {
        msg._private = (void *) (id);

        msg.keylen = snprintf(keybuf, sizeof(keybuf), "key%ju", id);
        msg.msglen = snprintf(payload, sizeof(payload), "msg%ju", id);

        ret = kafkatools_produce_timedwait(producer, site, &msg, 1000, 30);

        if (ret != KAFKATOOLS_SUCCESS) {
            const char * errstr = kafkatools_producer_get_errstr(producer, &ret);

            printf("kafkatools_produce_timedwait failed(%d): %s.\n", ret, errstr);

            break;
        }

        printf("<%.*s>: {%.*s}\n", (int)msg.keylen, msg.key, (int)msg.msglen, msg.msgbuf);

        ++id;
    }
}


// DELETE
int main2 (int argc, char *argv[])
{
    ub8 startid = 1;
    ub8 maxcount = 10000;

    if (argc == 3) {
        startid = strtoull(argv[1], 0, 0);
        maxcount = strtoull(argv[2], 0, 0);
    }

    printf("startid=%" PRIu64" maxcount=%" PRIu64"\n", startid, maxcount);

    do {
        kt_producer producer;

        kafkatools_msg_site_t site;

        /**
         * https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md
         */
        const char *names[] = {
            "bootstrap.servers",
            "socket.timeout.ms",
            "queue.buffering.max.messages",
            "message.max.bytes",
            0
        };

        const char *values[] = {
            "192.168.39.111:9092",
            "30000",
            "4000",
            "32768",
            0
        };

        int ret = kafkatools_producer_create(names, values, produce_state_cb, (void *) &site, &producer);
        if (ret != KAFKATOOLS_SUCCESS) {
            printf("kafkatools_producer_create failed(%d).\n", ret);
            exit(-1);
        }

        site.topic = kafkatools_producer_get_topic(producer, MYTOPIC);
        site.partition = 0;

        produce_messages(producer, &site, startid, maxcount);

        Sleep(10000);
        //?? kafkatools_producer_destroy(producer, KAFKATOOLS_WAIT_INFINITE);
    } while(0);

    return 0;
}
