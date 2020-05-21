/***********************************************************************
 * Copyright (c) 2008-2080 pepstack.com, 350137278@qq.com
 *
 * ALL RIGHTS RESERVED.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **********************************************************************/

/**
 * @filename   kafkatools_producer.c
 *  kafka producer api both for Windows and Linux.
 *
 * @refer
 *    https://github.com/edenhill/librdkafka/blob/master/src/rdkafka.h
 *    https://github.com/edenhill/librdkafka/blob/master/examples/rdkafka_simple_producer.c
 *
 * @author     Liang Zhang <350137278@qq.com>
 * @version    0.0.11
 * @create     2018-10-08
 * @update     2019-11-22 12:11:42
 */
#include "kafkatools.h"

#include <common/red_black_tree.h>
#include <common/readconf.h>
#include <common/memapi.h>
#include <common/misc.h>
#include <common/cstrbuf.h>

static const char THIS_FILE[] = "kafkatools_producer.c";


typedef struct kafkatools_producer_t
{
    pthread_mutex_t  lock;

    /* Producer instance handle */
    rd_kafka_t *rkProducer;

    void * opaque;

    red_black_tree_t  rktopic_tree;

    int errcode;
    char errstr[KAFKATOOLS_ERRSTR_SIZE];
} kafkatools_producer_t;


static int rktopic_name_cmp(void *newObject, void *nodeObject)
{
    return strcmp(rd_kafka_topic_name((rd_kafka_topic_t *) newObject), rd_kafka_topic_name((rd_kafka_topic_t *) nodeObject));
}


/*! Callback function prototype for traverse objects */
static void rktopic_object_release(void *object, void *param)
{
    rd_kafka_topic_destroy((rd_kafka_topic_t *) object);
}


/**
 * Message delivery report callback.
 *
 * This callback is called exactly once per message, indicating if the message
 *  was succesfully delivered (rkmessage->err == RD_KAFKA_RESP_ERR_NO_ERROR) or
 *  permanently failed delivery (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR).
 *
 * The callback is triggered from rd_kafka_poll() and executes on the
 *  application's thread.
 *
 * The rkmessage is destroyed automatically by librdkafka.
 */
static void kt_msg_cb_default (rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
    if (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        printf("[error] Message delivery failed: %s\n", rd_kafka_err2str(rkmessage->err));
    } else {
        // printf("success: Message delivered (%zd bytes, partition %"PRId32")\n", rkmessage->len, rkmessage->partition);
    }
}


static cstrbuf kt_get_producer_properties_pathfile (const char *propspathfile)
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
                        cstr_length(&propspathfile[2], KAFKATOOLS_PROPSFILE_LEN_MAX), &propspathfile[2]);
        cstrbufFree(&bindir);
        return propsfile;
    }

    if (propspathfile[0] == '.' && propspathfile[1] == '.' && (propspathfile[2] == PATH_SEPARATOR_CHAR || propspathfile[2] =='/')) {
        // parent dir:
        //   "../config/kafka-producer.properties"
        propsfile = cstrbufCat(0, "%.*s%c%.*s",
                    cstrbufGetLen(bindir), cstrbufGetStr(bindir), PATH_SEPARATOR_CHAR,
                    cstr_length(propspathfile, KAFKATOOLS_PROPSFILE_LEN_MAX), propspathfile);
        cstrbufFree(&bindir);
        return propsfile;
    }

    // absolute path
    cstrbufFree(&bindir);
    propsfile = cstrbufCat(0, "%.*s", cstr_length(propspathfile, KAFKATOOLS_PROPSFILE_LEN_MAX), propspathfile);
    return propsfile;
}


/**********************************************************
 * public api
 **********************************************************/

/**
 * high level api for kafka producer
 */
int kafkatools_producer_state_init (const char *propertiesfile, const char *topicpartitions, kafkatools_msg_cb statecb, void *argp, ktproducer_state_t *state)
{
    int ret;

    char *propsbuf = 0;
    size_t bufsize = 0;

    char *propnames[KAFKATOOLS_CONF_PROPS_MAX] = {0};
    char *propvalues[KAFKATOOLS_CONF_PROPS_MAX] = {0};
    char *topicptsplit[2] = {0};

    cstrbuf propsfile = kt_get_producer_properties_pathfile(propertiesfile);
    cstrbuf topicpt = cstrbufNew(0, topicpartitions, -1);

    if (!topicpt) {
        printf("(%s:%d) ERROR -  invalid topic partitions: '%s'\n",  THIS_FILE, __LINE__, topicpartitions);
        cstrbufFree(&propsfile);
        return KAFKATOOLS_EARG;
    }

    cstr_split_substr(topicpt->str, ":", 1, topicptsplit, 2);
    if (! topicptsplit[0]) {
        printf("(%s:%d) ERROR - bad topic partitions: '%s'\n", THIS_FILE, __LINE__, topicpartitions);
        cstrbufFree(&propsfile);
        cstrbufFree(&topicpt);
        return KAFKATOOLS_EFATAL;
    }

    if (! pathfile_exists(cstrbufGetStr(propsfile))) {
        printf("(%s:%d) ERROR -  properties file not found: '%.*s'\n",  THIS_FILE, __LINE__,
            cstrbufGetLen(propsfile), cstrbufGetStr(propsfile));

        cstrbufFree(&propsfile);
        cstrbufFree(&topicpt);
        return KAFKATOOLS_EFILE;
    }

    ret = kafkatools_props_readconf(cstrbufGetStr(propsfile), topicptsplit[0], &propsbuf, &bufsize);
    cstrbufFree(&propsfile);

    if (kafkatools_props_retrieve(propsbuf, bufsize, propnames, propvalues, ret) != KAFKATOOLS_SUCCESS) {
        printf("(%s:%d) ERROR - kafkatools_props_retrieve failed.\n", THIS_FILE, __LINE__);
        cstrbufFree(&topicpt);
        kafkatools_propsbuf_free(propsbuf);
        return KAFKATOOLS_EPROPS;
    }

    // create producer for kafka
    state->statearg = argp;
    ret = kafkatools_producer_create(propnames, propvalues, statecb, (void *)state, &state->producer);
    if (ret != KAFKATOOLS_SUCCESS) {
        printf("(%s:%d) ERROR - kafkatools_producer_create failed.\n", THIS_FILE, __LINE__);
        cstrbufFree(&topicpt);
        kafkatools_propsbuf_free(propsbuf);
        return KAFKATOOLS_ERROR;
    }
    kafkatools_propsbuf_free(propsbuf);

    // set topic and partitions scope
    if (topicptsplit[1]) {
        state->site.topic = kafkatools_producer_get_topic(state->producer, topicptsplit[0]);

        if (strchr(topicptsplit[1], '-')) {
            char *maxpartid = strchr(topicptsplit[1], '-');
            *maxpartid++ = 0;
            state->site.partitionid_min = atoi(topicptsplit[1]);
            state->site.partitionid_max = atoi(maxpartid);
        } else {
            // only one partition
            state->site.partition = state->site.partitionid_min = state->site.partitionid_max = atoi(topicptsplit[1]);
        }
    } else {
        // only topic without partition
        state->site.topic = kafkatools_producer_get_topic(state->producer, topicpt->str);

        state->site.partition = state->site.partitionid_min = state->site.partitionid_max = 0;
    }

    // validate state topic partitions
    if (! state->site.topic ||
        state->site.partitionid_max < state->site.partitionid_min ||
        state->site.partitionid_min < 0 ||
        state->site.partitionid_max > KAFKATOOLS_PARTITIONID_MAX) {
        printf("(%s:%d) ERROR - bad topic partitions: '%.*s'\n", THIS_FILE, __LINE__,
            cstrbufGetLen(topicpt), cstrbufGetStr(topicpt));

        cstrbufFree(&topicpt);
        return KAFKATOOLS_EARG;
    }

    cstrbufFree(&topicpt);
    return KAFKATOOLS_SUCCESS;
}


void kafkatools_producer_state_uninit (ktproducer_state_t *state)
{
    // TODO:
    //why error: kafkatools_producer_destroy(state->producer, KAFKATOOLS_WAIT_INFINITE);
}


/**
 * public helper api
 */
const char * kafkatools_get_rdkafka_version (void)
{
    return rd_kafka_version_str();
}


int kafkatools_mutex_lock (pthread_mutex_t * mutex, int is_try)
{
    return (is_try? pthread_mutex_trylock(mutex) : pthread_mutex_lock(mutex));
}


void kafkatools_mutex_unlock (pthread_mutex_t * mutex)
{
    pthread_mutex_unlock(mutex);
}


int kafkatools_props_readconf (const char *conf_file, const char *section, char **propsbuf, size_t *bufsz)
{
    char *sec, *key, *val, *outbuf;

    int props = 0;
    size_t cb, outsz = 0;
    size_t offset;

    CONF_position cpos = ConfOpenFile(conf_file);
    if (! cpos) {
        return KAFKATOOLS_EFILE;
    }

    // calc size for props in section
    sec = ConfGetFirstPair(cpos, &key, &val);
    while (sec) {
        if (! section || ! strcmp(ConfGetSection(cpos), section)) {
            props++;
            outsz += strlen(key) + 1 + strlen(val) + 1;
        }
        if (props >= KAFKATOOLS_CONF_PROPS_MAX) {
            ConfCloseFile(cpos);
            return KAFKATOOLS_EPROPS;
        }
        sec = ConfGetNextPair(cpos, &key, &val);
    }

    if (! outsz) {
        ConfCloseFile(cpos);
        return 0;
    }

    outbuf = mem_alloc_unset(outsz + 1);
    if (! outbuf) {
        ConfCloseFile(cpos);
        return KAFKATOOLS_ENOMEM;
    }
    outbuf[outsz] = 0;

    // copy all props in section
    offset = 0;
    sec = ConfGetFirstPair(cpos, &key, &val);
    while (sec) {
        if (! section || ! strcmp(ConfGetSection(cpos), section)) {
            cb = strlen(key) + 1;
            if (offset + cb > outsz) {
                mem_free(outbuf);
                ConfCloseFile(cpos);
                return KAFKATOOLS_EBREAK;
            }
            bcopy(key, outbuf + offset, cb);
            offset += cb;

            cb = strlen(val) + 1;
            if (offset + cb > outsz) {
                mem_free(outbuf);
                ConfCloseFile(cpos);
                return KAFKATOOLS_EBREAK;
            }
            bcopy(val, outbuf + offset, cb);
            offset += cb;
        }
        sec = ConfGetNextPair(cpos, &key, &val);
    }

    ConfCloseFile(cpos);

    if (offset != outsz) {
        mem_free(outbuf);
        return KAFKATOOLS_EBREAK;
    }

    *propsbuf = outbuf;
    *bufsz = outsz;
    return props;
}


int kafkatools_props_retrieve (const char *propsbuf, size_t bufsz, char **propnames, char **propvalues, int numprops)
{
    int i;
    size_t cb;

    size_t offset = 0;
    char *p = (char *) propsbuf;

    for (i = 0; i < numprops; i++) {
        propnames[i] = p;

        cb = strlen(p) + 1;
        offset += cb;
        p += cb;

        propvalues[i] = p;

        cb = strlen(p) + 1;
        offset += cb;
        p += cb;

        if (offset > bufsz) {
            return KAFKATOOLS_ERROR;
        }
    }

    propnames[i] = 0;
    propvalues[i] = 0;

    return KAFKATOOLS_SUCCESS;
}


void kafkatools_propsbuf_free (char *propsbuf)
{
    mem_free(propsbuf);
}


int kafkatools_producer_create (char **propnames, char **propvalues, kafkatools_msg_cb msg_cb, void *msg_opaque, kt_producer *outproducer)
{
    int i;

    int result;

    rd_kafka_conf_res_t res;

    rd_kafka_conf_t * conf = 0;
    kafkatools_producer_t * producer = 0;

    result = KAFKATOOLS_ENOMEM;

    producer = (kt_producer) mem_alloc_unset(sizeof(*producer));
    if (! producer) {
        goto on_error_result;
    }
    bzero(producer, sizeof(*producer));

    rbtree_init(&producer->rktopic_tree, (rbtreenode_cmp_func *) rktopic_name_cmp);

    /*
     * Create Kafka client configuration place-holder
     */
    conf = rd_kafka_conf_new();
    if (! conf) {
        goto on_error_result;
    }

    if (propnames) {
        /* Set bootstrap broker(s) as a comma-separated list of
         *  host or host:port (default port 9092).
         *  librdkafka will use the bootstrap brokers to acquire the full
         *  set of brokers from the cluster.
         *
         *  char *names[] = {
         *      "bootstrap.servers",
         *      "socket.timeout.ms",
         *      0
         *  };
         *
         *  char *values[] = {
         *     "localhost:9092,localhost2:9092",
         *     "1000",
         *  };
         */
        i = 0;
        while (i < KAFKATOOLS_CONF_PROPS_MAX && propnames[i]) {
            res = rd_kafka_conf_set(conf, propnames[i], propvalues[i], producer->errstr, sizeof(producer->errstr));

            if (res != RD_KAFKA_CONF_OK) {
                perror(producer->errstr);
                result = KAFKATOOLS_ECONF;
                goto on_error_result;
            }

            ++i;
        }
    }

    /* Set the delivery report callback.
     * This callback will be called once per message to inform the application
     *  if delivery succeeded or failed. See dr_msg_cb() above.
     */
    if (msg_cb == KAFKATOOLS_MSG_CB_DEFAULT) {
        rd_kafka_conf_set_dr_msg_cb(conf, kt_msg_cb_default);
    } else {
        rd_kafka_conf_set_dr_msg_cb(conf, msg_cb);
    }

    /* Sets the application's opaque pointer that will be passed to callbacks */
    rd_kafka_conf_set_opaque(conf, msg_opaque);

    /*
     * Create producer instance.
     *
     * NOTE: rd_kafka_new() takes ownership of the conf object
     *       and the application must not reference it again after
     *       this call.
     */
    producer->rkProducer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, producer->errstr, sizeof(producer->errstr));

    if (! producer->rkProducer) {
        result = KAFKATOOLS_ERROR;
        goto on_error_result;
    }

    /*
     * https://linux.die.net/man/3/pthread_mutex_init
     */
    if (pthread_mutex_init(&producer->lock, NULL) != 0) {
        result = KAFKATOOLS_EFATAL;
        goto on_error_result;
    }

    producer->opaque = msg_opaque;
    *outproducer = producer;
    return KAFKATOOLS_SUCCESS;

on_error_result:
    kafkatools_producer_destroy(producer, 0);

    if (conf) {
        rd_kafka_conf_destroy(conf);
    }

    return result;
}


void kafkatools_producer_destroy (kt_producer producer, int flush_wait_ms)
{
    if (producer) {
        if (producer->rkProducer) {
            rd_kafka_t *rkProducer = producer->rkProducer;

            producer->rkProducer = 0;

            /*  Wait until all outstanding produce requests, et.al, are completed. */
            while (flush_wait_ms-- != 0) {
                if (rd_kafka_flush(rkProducer, 1) == RD_KAFKA_RESP_ERR__TIMED_OUT) {
                    continue;
                }

                break;
            }

            /* Destroy the producer instance */
            rd_kafka_destroy(rkProducer);
        }

        rbtree_traverse(&producer->rktopic_tree, rktopic_object_release, 0);
        rbtree_clean(&producer->rktopic_tree);

        pthread_mutex_destroy(&producer->lock);

        mem_free(producer);
    }
}


const char * kafkatools_producer_get_errstr (kt_producer producer, int *perrcode)
{
    producer->errstr[ sizeof(producer->errstr) - 1 ] = '\0';

    if (perrcode) {
        *perrcode = producer->errcode;
    }

    return producer->errstr;
}


rd_kafka_t * kafkatools_producer_get_rdkafka (kt_producer producer)
{
    return producer->rkProducer;
}


void * kafkatools_producer_get_opaque (kt_producer producer)
{
    return producer->opaque;
}


pthread_mutex_t * kafkatools_producer_get_mutex (kt_producer producer)
{
    return &(producer->lock);
}


kt_topic kafkatools_producer_get_topic (kt_producer producer, const char *topic_name)
{
    red_black_node_t *node;

    /* Topic handles are refcounted internally and calling rd_kafka_topic_new()
     *  again with the same topic name will return the previous topic handle
     *  without updating the original handle's configuration.
     *
     * Applications must eventually call rd_kafka_topic_destroy() for each
     *  succesfull call to rd_kafka_topic_new() to clear up resources.
     *
     * returns the new topic handle or NULL on error.
     */
    rd_kafka_topic_t *rktopic;

    rktopic = rd_kafka_topic_new(producer->rkProducer, topic_name, NULL);

    if (! rktopic) {
        snprintf_chkd_V1(producer->errstr, KAFKATOOLS_ERRSTR_SIZE, "rd_kafka_topic_new(topic=%s) fail: %s",
            topic_name, rd_kafka_err2str(rd_kafka_last_error()));
        return NULL;
    }

    node = rbtree_find(&producer->rktopic_tree, (void*) rktopic);
    if (! node) {
        // 如果 tree 没有 topic, 插入新 topic
        int is_new_node;

        node = rbtree_insert_unique(&producer->rktopic_tree, (void *)rktopic, &is_new_node);
        if (! node || ! is_new_node) {
            // 必须成功
            printf("application error: should never run to this!\n");
            exit(-1);
        }
    } else {
        // 如果 tree 中已经存在 topic, 释放 topic 引用计数
        rd_kafka_topic_destroy(rktopic);
    }

    // do not call rd_kafka_topic_destroy() for below object!
    return (kt_topic) rktopic;
}


const char * kafkatools_topic_name (const kt_topic topic)
{
    return rd_kafka_topic_name((const rd_kafka_topic_t *) topic);
}


int kafkatools_produce_timedwait (kt_producer producer, kafkatools_msg_site_t *ktsite, kafkatools_msg_data_t *ktmsg, int timout_ms, int retry_count)
{
    int ret = 0;

    while (retry_count-- != 0) {
        ret = rd_kafka_produce((rd_kafka_topic_t *) ktsite->topic,
                ktsite->partition,
                RD_KAFKA_MSG_F_COPY,         /* Make a copy of the payload. */
                (void* ) ktmsg->msgbuf, ktmsg->msglen,        /* Message payload (value) and length */
                ktmsg->key, ktmsg->keylen,   /* Optional key and its length for partition */
                ktmsg->_private);            /* msg_opaque is an optional application-provided per-message opaque
                                              *  pointer that will provided in the delivery report callback (`dr_cb`) for
                                              *  referencing this message. */
        if (ret == -1) {
            /**
             *  - ENOBUFS  - maximum number of outstanding messages has been reached:
             *               "queue.buffering.max.messages"
             *               (RD_KAFKA_RESP_ERR__QUEUE_FULL)
             *  - EMSGSIZE - message is larger than configured max size:
             *               "message.max.bytes".
             *               (RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE)
             *  - ESRCH    - requested \p partition is unknown in the Kafka cluster.
             *               (RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION)
             *  - ENOENT   - topic is unknown in the Kafka cluster.
             *               (RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC)
             *  - ECANCELED - fatal error has been raised on producer, see
             *                rd_kafka_fatal_error().
             */
            if (rd_kafka_last_error() == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
                /* If the internal queue is full, wait for messages to be delivered and then retry.
                 * The internal queue represents both messages to be sent and messages that have
                 *  been sent or failed, awaiting their delivery report callback to be called.
                 *
                 * The internal queue is limited by the configuration property:
                 *      queue.buffering.max.messages
                 */
                rd_kafka_poll(producer->rkProducer, timout_ms);

                continue;
            }
        }

        break;
    }

    if (ret == -1) {
        /* unexpect error */
        producer->errcode = rd_kafka_last_error();
        snprintf(producer->errstr, sizeof(producer->errstr), "rd_kafka_produce {%s:%d} failed(%d): %s",
            kafkatools_topic_name(ktsite->topic), ktsite->partition, producer->errcode, rd_kafka_err2str(producer->errcode));
        return KAFKATOOLS_ERROR;
    }

    return KAFKATOOLS_SUCCESS;
}

