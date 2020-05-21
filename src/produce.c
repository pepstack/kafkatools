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
static void producer_notify_state (rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *statep)
{
    ub8 id = (ub8) (rkmessage->_private);
    ktproducer_state_t *state = (ktproducer_state_t *) statep;

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

    // Usage:
    //   kafkatools_producer_state_init(NULL, "test:0-7", notify_state_cb, NULL, &state);
    ret = kafkatools_producer_state_init((argc == 3? argv[2] : 0), (argc > 1? argv[1] : MYTOPIC), producer_notify_state, NULL, &state);
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

    kafkatools_producer_state_uninit(&state, KAFKATOOLS_WAIT_INFINITE);
    return 0;
}
