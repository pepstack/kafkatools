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
static void produce_msg_cb (rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque)
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

static cstrbuf parse_properties_path (const char *propspathfile) 
{
    cstrbuf propsfile = 0;
    cstrbuf bindir = get_proc_abspath();

    if (! propspathfile) {
        // using default file
        propsfile = cstrbufCat(0, "%.*s%ckafka-producer.properties",
                    cstrbufGetLen(bindir), cstrbufGetStr(bindir), PATH_SEPARATOR_CHAR);
        cstrbufFree(&bindir);
        return propsfile;
    }

    if (propspathfile[0] == '.' && (propspathfile[1] == PATH_SEPARATOR_CHAR || propspathfile[1] == '/')) {
        // current app dir:
        //   "./config/kafka-producer.properties"
        propsfile = cstrbufCat(0, "%.*s%c%.*s", cstrbufGetLen(bindir), cstrbufGetStr(bindir), PATH_SEPARATOR_CHAR,
                        cstr_length(&propspathfile[2], PROPSFILE_LEN_MAX), &propspathfile[2]);
        cstrbufFree(&bindir);
        return propsfile;
    }

    if (propspathfile[0] == '.' && propspathfile[1] == '.' && (propspathfile[2] == PATH_SEPARATOR_CHAR || propspathfile[2] =='/')) {
        // parent dir:
        //   "../config/kafka-producer.properties"
        propsfile = cstrbufCat(0, "%.*s%c%.*s",
                    cstrbufGetLen(bindir), cstrbufGetStr(bindir), PATH_SEPARATOR_CHAR,
                    cstr_length(propspathfile, PROPSFILE_LEN_MAX), propspathfile);
        cstrbufFree(&bindir);
        return propsfile;
    }

    // absolute path
    cstrbufFree(&bindir);
    propsfile = cstrbufCat(0, "%.*s", cstr_length(propspathfile, PROPSFILE_LEN_MAX), propspathfile);
    return propsfile;
}


typedef struct {
    kt_producer producer;
    kafkatools_msg_site_t site;
} produce_state_t;


int main (int argc, char *argv[])
{
    cstrbuf propsfile = 0;

    // "$topic:$frompartitionid-$endpartitionid"
    cstrbuf topicpt = 0;

    char *propsbuf = 0;
    size_t bufsize = 0;
    int ret = 0;

    char *propnames[KAFKATOOLS_CONF_PROPS_MAX] = {0};
    char *propvalues[KAFKATOOLS_CONF_PROPS_MAX] = {0};

    produce_state_t state = {0};

    WINDOWS_CRTDBG_ON

    // load producer properties file
    //
    if (argc > 2) {
        propsfile = parse_properties_path(argv[2]);
        topicpt = cstrbufNew(0, argv[1], -1);
    } else {
        propsfile = parse_properties_path(0);
        topicpt = cstrbufNew(0, "test", -1);
    }

    printf("[info] kafka topic partitions: %.*s\n", cstrbufGetLen(topicpt), cstrbufGetStr(topicpt));
    printf("[info] using producer properties file: %.*s\n", cstrbufGetLen(propsfile), cstrbufGetStr(propsfile));

    if (! pathfile_exists(cstrbufGetStr(propsfile))) {
        printf("[fatal] producer properties file not found: %.*s\n", cstrbufGetLen(propsfile), cstrbufGetStr(propsfile));
        cstrbufFree(&propsfile);
        exit(EXIT_FAILURE);
    }

    ret = kafkatools_props_readconf(cstrbufGetStr(propsfile), cstrbufGetStr(topicpt), &propsbuf, &bufsize);
    cstrbufFree(&propsfile);

    if (kafkatools_props_retrieve(propsbuf, bufsize, propnames, propvalues, ret) != KAFKATOOLS_SUCCESS) {
        printf("[fatal] bad producer properties file.\n");
        kafkatools_propsbuf_free(propsbuf);
        exit(EXIT_FAILURE);
    }

    // create producer for kafka
    //
    ret = kafkatools_producer_create(propnames, propvalues, produce_msg_cb, (void *) &state.site, &state.producer);
    if (ret != KAFKATOOLS_SUCCESS) {
        printf("[fatal] kafkatools_producer_create failed(%d).\n", ret);
        kafkatools_propsbuf_free(propsbuf);
        exit(EXIT_FAILURE);
    }
    kafkatools_propsbuf_free(propsbuf);

    char *topicptsplit[2] = {0};

    ret = cstr_split_substr(topicpt->str, ":", 1, topicptsplit, 2);
    if (ret == 1) {
        // only topic without partition
        state.site.topic = kafkatools_producer_get_topic(state.producer, topicpt->str);

        state.site.partition = state.site.partitionid_min = state.site.partitionid_max = 0;
    } else if (ret == 2) {
        state.site.topic = kafkatools_producer_get_topic(state.producer, topicptsplit[0]);

        if (strchr(topicptsplit[1], '-')) {
            char *maxpartid = strchr(topicptsplit[1], '-');
            *maxpartid++ = 0;
            state.site.partitionid_min = atoi(topicptsplit[1]);
            state.site.partitionid_max = atoi(maxpartid);
        } else {
            // only one partition
            state.site.partition = state.site.partitionid_min = state.site.partitionid_max = atoi(topicptsplit[1]);
        }
    } else {
        printf("[fatal] bad topic partitions.\n");
        exit(EXIT_FAILURE);
    }

    if (! state.site.topic ||
        state.site.partitionid_max < state.site.partitionid_min ||
        state.site.partitionid_min < 0 ||
        state.site.partitionid_max > KAFKATOOLS_PARTITIONID_MAX) {
        printf("[fatal] bad topic partitions.\n");
        exit(EXIT_FAILURE);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // You can use this "state" in your message produce thread as below:
    do {
        void * thrarg = (void*) &state;
        int err;

        produce_state_t * thrstate = (produce_state_t *) thrarg;

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

        err = kafkatools_produce_timedwait(thrstate->producer, &thrstate->site, &msg, 1000, 30);
        if (err != KAFKATOOLS_SUCCESS) {
            const char * errstr = kafkatools_producer_get_errstr(thrstate->producer, &err);
            // LOGGER_ERROR(...);
        }
    } while(1);


    // TODO: kafkatools_producer_destroy(producer, KAFKATOOLS_WAIT_INFINITE);
    return 0;
}


///////////////////////////////////////////////////////////////

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

        int ret = kafkatools_producer_create(names, values, produce_msg_cb, (void *) &site, &producer);
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
