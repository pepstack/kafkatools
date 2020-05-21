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
 * @filename   kafkatools_consumer.c
 *  kafka consumer api both for Windows and Linux.
 *
 * @author     Liang Zhang <350137278@qq.com>
 * @version    0.0.11
 * @create     2017-12-20
 * @update     2020-05-20 11:32:50
 */
#include "kafkatools.h"

#include <common/red_black_tree.h>
#include <common/readconf.h>
#include <common/memapi.h>
#include <common/misc.h>
#include <common/cstrbuf.h>


static const char THIS_FILE[] = "kafkatools_consumer.c";

/**
 * API doc:
 *   https://docs.confluent.io/2.0.0/clients/librdkafka/rdkafka_8h.html
 *   https://github.com/edenhill/librdkafka/blob/master/src/rdkafka.h
 */

typedef struct kafkatools_consumer_t
{
    pthread_mutex_t  lock;

    /* Consumer instance handle */
    rd_kafka_t *rkConsumer;

    void * opaque;

    red_black_tree_t  rktopic_tree;

    int errcode;
    char errstr[KAFKATOOLS_ERRSTR_SIZE];

    /* Topic+Partition container */
    rd_kafka_topic_partition_list_t * tp_list;
} kafkatools_consumer_t;


static int rktopic_name_cmp(void *newObject, void *nodeObject)
{
    return strcmp(rd_kafka_topic_name((rd_kafka_topic_t *) newObject), rd_kafka_topic_name((rd_kafka_topic_t *) nodeObject));
}


/*! Callback function prototype for traverse objects */
static void rktopic_object_release(void *object, void *param)
{
    rd_kafka_topic_destroy((rd_kafka_topic_t *) object);
}


int kafkatools_consumer_create (const char *groupid, const char *brokers, int tplist_size, const char * names[], const char * values[], const char * topics[], void * opaque, kt_consumer *outConsumer)
{
    int result;

    char errstr[KAFKATOOLS_ERRSTR_SIZE];

    rd_kafka_conf_res_t res;

    rd_kafka_conf_t * conf = 0;
    rd_kafka_topic_conf_t * topic_conf = 0;

    rd_kafka_t * rkConsumer = 0;
    rd_kafka_topic_partition_list_t * tp_list = 0;

    kafkatools_consumer_t * consumer = 0;

    result = KAFKATOOLS_ENOMEM;

    consumer = (kafkatools_consumer_t *) mem_alloc_zero(1, sizeof(*consumer));
    if (! consumer) {
        goto on_error_result;
    }

    /* success returns consumer object */
    rbtree_init(&consumer->rktopic_tree, (rbtreenode_cmp_func *) rktopic_name_cmp);

    /* Use rd_kafka_topic_partition_list_destroy()
     *  to free all resources in use by a list and the list itself.
     *    tplist_size - Initial allocated size.
     */
    tp_list = rd_kafka_topic_partition_list_new(tplist_size);
    if (! tp_list) {
        goto on_error_result;
    }

    if (topics) {
        const char *tp;

        int i = 0;

        while ((i < KAFKATOOLS_TOPICS_MAX) && ((tp = topics[i]) != 0)) {
            /**
             * topicName:startPartitionid-endPartitionid
             *
             * topicA:1-2
             * topicB:1
             */
            const char * id = strchr(tp, ':');

            if (! id) {
                rd_kafka_topic_partition_list_add(tp_list, tp, -1);
            } else {
                char topic[KAFKATOOLS_TOPIC_NAMELEN + 1];
                const char * q = strchr(id, '-');

                if (id - tp > KAFKATOOLS_TOPIC_NAMELEN) {
                    result = KAFKATOOLS_EARG;
                    goto on_error_result;
                }

                bcopy(tp, topic, id - tp);
                topic[id - tp] = 0;

                if (! q) {
                    rd_kafka_topic_partition_list_add(tp_list, topic, atoi(id));
                } else {
                    int startid, stopid;

                    char p[11];

                    if (q - id >= sizeof(p)) {
                        result = KAFKATOOLS_EARG;
                        goto on_error_result;
                    }

                    bcopy(id, p, q - id);
                    p[q++ - id] = 0;

                    startid = atoi(p);
                    stopid = atoi(q);

                    if (startid < stopid) {
                        rd_kafka_topic_partition_list_add_range(tp_list, topic, startid, stopid);
                    } else {
                        rd_kafka_topic_partition_list_add_range(tp_list, topic, stopid, startid);
                    }
                }
            }

            ++i;
        }
    }

    conf = rd_kafka_conf_new();
    if (! conf) {
        goto on_error_result;
    }

    topic_conf = rd_kafka_topic_conf_new();
    if (! topic_conf) {
        goto on_error_result;
    }

    /* Sets the application's opaque pointer that will be passed to all topic
     * callbacks as the rkt_opaque argument.
     */
    rd_kafka_topic_conf_set_opaque(topic_conf, opaque);

    /* Emit RD_KAFKA_RESP_ERR__PARTITION_EOF event whenever the consumer reaches
     * the end of a partition.
     */
    rd_kafka_conf_set(conf, "enable.partition.eof", "true", NULL, 0);

    if (names) {
        int i = 0;

        while (i < KAFKATOOLS_CONF_PROPS_MAX && names[i]) {
            res = RD_KAFKA_CONF_UNKNOWN;

            /* Try "topic." prefixed properties on topic conf first, and then fall through
             * to global if it didnt match a topic configuration property.
             */
            if (! strncmp(names[i], "topic.", 6)) {
                res = rd_kafka_topic_conf_set(topic_conf, names[i] + 6, values[i], errstr, sizeof(errstr));
            }

            if (res == RD_KAFKA_CONF_UNKNOWN) {
                res = rd_kafka_conf_set(conf, names[i], values[i], errstr, sizeof(errstr));
            }

            if (res != RD_KAFKA_CONF_OK) {
                result = KAFKATOOLS_ECONF;
                goto on_error_result;
            }

            ++i;
        }
    }

    /* Callback called on partition assignment changes */
    //rd_kafka_conf_set_rebalance_cb(conf, rebalance_cb);

    /* Consumer groups require a group id */
    if (groupid) {
        if (rd_kafka_conf_set(conf, "group.id", groupid, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            result = KAFKATOOLS_ECONF;
            goto on_error_result;
        }
    }

    /* Set default topic config for pattern-matched topics. */
    rd_kafka_conf_set_default_topic_conf(conf, topic_conf);

    /* The topic config object is not usable after this call. */
    topic_conf = 0;

    /* Create Kafka handle */
    rkConsumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));

    if (! rkConsumer) {
        result = KAFKATOOLS_ENOMEM;
        goto on_error_result;
    }

    /* conf object is freed by this function on success and must not be used
     * or destroyed by the application sub-sequently. */
    conf = 0;

    result = KAFKATOOLS_ERROR;

    /* Add brokers if needed */
    if (brokers) {
        if (rd_kafka_brokers_add(rkConsumer, brokers) == 0) {
            goto on_error_result;
        }
    }

    /* Redirect the main (rd_kafka_poll()) queue to the KafkaConsumer's
     *   queue (rd_kafka_consumer_poll()).
     * It is not permitted to call rd_kafka_poll() after directing the
     *   main queue with rd_kafka_poll_set_consumer().
     */
    res = rd_kafka_poll_set_consumer(rkConsumer);
    if (res != RD_KAFKA_CONF_OK) {
        goto on_error_result;
    }

    /* hand over kafka objects */
    consumer->rkConsumer = rkConsumer;
    rkConsumer = 0;

    consumer->tp_list = tp_list;
    tp_list = 0;

    /*
     * https://linux.die.net/man/3/pthread_mutex_init
     */
    if (pthread_mutex_init(&consumer->lock, NULL) != 0) {
        result = KAFKATOOLS_EFATAL;
        goto on_error_result;
    }

    consumer->opaque = opaque;
    *outConsumer = consumer;
    return KAFKATOOLS_SUCCESS;

on_error_result:

    kafkatools_consumer_destroy(consumer);

    if (tp_list) {
        rd_kafka_topic_partition_list_destroy(tp_list);
    }
    if (topic_conf) {
        rd_kafka_topic_conf_destroy(topic_conf);
    }
    if (conf) {
        rd_kafka_conf_destroy(conf);
    }
    if (rkConsumer) {
        rd_kafka_destroy(rkConsumer);
    }

    return result;
}


void kafkatools_consumer_destroy (kt_consumer consumer)
{
    if (consumer) {
        rd_kafka_t *rkConsumer = NULL;

        pthread_mutex_lock(&consumer->lock);

        if (consumer->rkConsumer) {
            rkConsumer = consumer->rkConsumer;
            consumer->rkConsumer = NULL;
        }

        if (consumer->tp_list) {
            rd_kafka_topic_partition_list_destroy(consumer->tp_list);
        }

        rbtree_traverse(&consumer->rktopic_tree, rktopic_object_release, 0);
        rbtree_clean(&consumer->rktopic_tree);

        if (rkConsumer) {
            /* Destroy Kafka handle. This is a blocking operation.
             *   rd_kafka_consumer_close() will be called from this function.
             */
            rd_kafka_destroy(rkConsumer);
        }

        pthread_mutex_destroy(&consumer->lock);
        mem_free(consumer);
    }
}


const char * kafkatools_consumer_get_errstr (kt_consumer consumer, int *perrcode)
{
    if (perrcode) {
        *perrcode = consumer->errcode;
    }

    return consumer->errstr;
}


rd_kafka_topic_conf_t * kafkatools_consumer_new_topic_conf (kt_consumer consumer, const char * names[], const char * values[])
{
    rd_kafka_topic_conf_t * topic_conf = rd_kafka_topic_conf_new();

    if (! topic_conf) {
        consumer->errcode = KAFKATOOLS_ENOMEM;
        snprintf(consumer->errstr, sizeof(consumer->errstr), "rd_kafka_topic_conf_new error(%d): Out memory", KAFKATOOLS_ENOMEM);
        return NULL;
    }

    if (names) {
        rd_kafka_conf_res_t res;

        int i = 0;

        while (i < KAFKATOOLS_CONF_PROPS_MAX && names[i]) {
            res = rd_kafka_topic_conf_set(topic_conf, names[i], values[i], consumer->errstr, sizeof(consumer->errstr));

            if (res != RD_KAFKA_CONF_OK) {
                consumer->errcode = KAFKATOOLS_ECONF;

                rd_kafka_topic_conf_destroy(topic_conf);

                return NULL;
            }

            ++i;
        }
    }

    consumer->errcode = KAFKATOOLS_SUCCESS;
    *consumer->errstr = 0;

    return topic_conf;
}


kt_topic kafkatools_consumer_get_topic (kt_consumer consumer, const char *topic_name, rd_kafka_topic_conf_t *topic_conf)
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

    rktopic = rd_kafka_topic_new(consumer->rkConsumer, topic_name, topic_conf);

    if (! rktopic) {
        consumer->errcode = KAFKATOOLS_ENOMEM;
        snprintf(consumer->errstr, sizeof(consumer->errstr), "rd_kafka_topic_new error(%d): Out memory", KAFKATOOLS_ENOMEM);

        return NULL;
    }

    node = rbtree_find(&consumer->rktopic_tree, (void*) rktopic);
    if (! node) {
        /* 如果 tree 没有 topic, 插入新 topic */
        int is_new_node;

        node = rbtree_insert_unique(&consumer->rktopic_tree, (void *) rktopic, &is_new_node);
        if (! node || ! is_new_node) {
            rd_kafka_topic_destroy(rktopic);

            consumer->errcode = KAFKATOOLS_ERROR;
            snprintf(consumer->errstr, sizeof(consumer->errstr), "rbtree_insert_unique error(%d): Unexpect", KAFKATOOLS_ERROR);

            return NULL;
        }
    } else {
        /* 如果 tree 中已经存在 topic, 释放 topic 引用计数 */
        rd_kafka_topic_destroy(rktopic);
    }

    /* do not call rd_kafka_topic_destroy() for below object! */
    return (kt_topic) rktopic;
}


rd_kafka_topic_partition_list_t * kafkatools_consumer_get_topics (kt_consumer consumer)
{
    return consumer->tp_list;
}


rd_kafka_t * kafkatools_consumer_get_rdkafka (kt_consumer consumer)
{
    return consumer->rkConsumer;
}


void * kafkatools_consumer_get_opaque (kt_consumer consumer)
{
    return consumer->opaque;
}


pthread_mutex_t * kafkatools_consumer_get_mutex (kt_consumer consumer)
{
    return &(consumer->lock);
}


int kafkatools_consumer_start_subscribe (kt_consumer consumer)
{
    /* rd_kafka_subscribe() is an asynchronous method which returns immediately.
     *   background threads will (re)join the group, wait for group rebalance,
     *   issue any registered rebalance_cb, assign() the assigned partitions,
     *   and then start fetching messages.
     * This cycle may take up to
     *   session.timeout.ms * 2 or more to complete.
     */
    if (consumer->errcode == RD_KAFKA_RESP_ERR_NO_ERROR) {
        return KAFKATOOLS_SUCCESS;
    } else {
        const char *str = rd_kafka_err2str(consumer->errcode);

        if (str) {
            snprintf(consumer->errstr, sizeof(consumer->errstr), "%s", str);
        } else {
            snprintf(consumer->errstr, sizeof(consumer->errstr), "unknown rdkafka error");
        }

        return KAFKATOOLS_ERROR;
    }
}


void kafkatools_list_topic_partitions (rd_kafka_topic_partition_list_t *partitions, kt_tplist_cb tpcb, void *cbarg)
{
    int i;

    for (i = 0 ; i < partitions->cnt ; i++) {
        rd_kafka_topic_partition_t * tp = &partitions->elems[i];

        if (! tpcb(tp, cbarg)) {
            break;
        }
    }
}
