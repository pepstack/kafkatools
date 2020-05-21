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
 * @filename   kafkatools.h
 *  kafka api.
 *
 * Api doc:
 *   https://docs.confluent.io/2.0.0/clients/librdkafka/rdkafka_8h.html
 *
 * How to use:
 *   http://www.yolinux.com/TUTORIALS/LibraryArchives-StaticAndDynamic.html
 *
 *   http://www.lehoon.cn/backend/2018/03/14/librdkafka-config.html
 *
 *   https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md
 *
 * NOTES:
 *   librdkafka is THREAD-SAFE and you can use the same producer or consumer
 *     client from multiple threads.
 *
 * @author     Liang Zhang <350137278@qq.com>
 * @version    0.0.9
 * @create     2018-10-08 16:17:00
 * @update     2020-05-20 11:32:50
 */
#ifndef KAFKATOOLS_H_INCLUDED
#define KAFKATOOLS_H_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The C API is also documented in rdkafka.h
 */
#include <common/unitypes.h>

#ifdef __WINDOWS__
# include <rdkafka.h>
#else
# include <librdkafka/rdkafka.h>
#endif

/* using pthread or pthread-w32 */
#include <sched.h>
#include <pthread.h>


#define KAFKATOOLS_SUCCESS    0
#define KAFKATOOLS_ERROR    (-1)

#define KAFKATOOLS_ECONF    (-2)
#define KAFKATOOLS_EARG     (-3)
#define KAFKATOOLS_ENOMEM   (-4)
#define KAFKATOOLS_EFILE    (-5)
#define KAFKATOOLS_EBREAK   (-6)
#define KAFKATOOLS_EPROPS   (-7)
#define KAFKATOOLS_EFATAL   (-10)

#define KAFKATOOLS_WAIT_INFINITE      (-1)
#define KAFKATOOLS_CONF_PROPS_MAX      256
#define KAFKATOOLS_TOPICS_MAX         1024
#define KAFKATOOLS_PARTITIONID_MAX    4095
#define KAFKATOOLS_TOPIC_NAMELEN       127
#define KAFKATOOLS_ERRSTR_SIZE         256
#define KAFKATOOLS_PROPSFILE_LEN_MAX   255


#define KAFKATOOLS_MSG_CB_DEFAULT  ((kafkatools_msg_cb)((void*) (uintptr_t) (int) (-1)))

typedef void (*kafkatools_msg_cb) (rd_kafka_t *rk,  const rd_kafka_message_t *rkmessage, void *opaque);

typedef struct kafkatools_producer_t * kt_producer;

typedef struct kafkatools_consumer_t * kt_consumer;

typedef rd_kafka_topic_t * kt_topic;


typedef struct kafkatools_msg_site_t
{
    /* Topic object */
    kt_topic topic;

    /* Use builtin partitioner (RD_KAFKA_PARTITION_UA) to select partition */
    int32_t partition;

    /* constant partitionid scope: [min, max] */
    int32_t partitionid_min;
    int32_t partitionid_max;
} kafkatools_msg_site_t;


typedef struct kafkatools_msg_data_t
{
    /* Message payload (msg) and length(msglen) */
    ssize_t msglen;
    char *msgbuf;

    /* Optional key and its length */
    ssize_t keylen;
    char *key;

    /* Message opaque, provided in delivery report callback as _private of rd_kafka_message_t */
    void * _private;
} kafkatools_msg_data_t;


typedef struct kafkatools_producer_api_t
{
    void *handle;

    kt_producer producer;

    const char * (* kt_get_rdkafka_version) (void);
    const char * (* kt_producer_get_errstr) (kt_producer);
    int (* kt_producer_create) (const char **, const char **, kafkatools_msg_cb, void *, kt_producer *);
    void (* kt_producer_destroy) (kt_producer);
    kt_topic (* kt_get_topic) (kt_producer, const char *);
    const char * (* kt_topic_name) (const kt_topic);
    int (*kt_produce_message_sync) (kt_producer, const char *, int, kt_topic, int, int);
} kafkatools_producer_api_t;


/**
 * high level api for kafka producer
 */
typedef struct
{
    kt_producer producer;
    kafkatools_msg_site_t site;
    void *statearg;
} ktproducer_state_t;


/**
 * create kafka producer state from properties file
 *   `propertiesfile` - path file to kafka producer properties
 *   `topicpartitions` - topic with  partitionid scope like below:
 *        "$topic:$partitionidMin-$partitionidMax"
 *        "test:0-63" means topic is test and partitionid scope is [0, 63]
 */
extern int kafkatools_producer_state_init (const char *propertiesfile, const char *topicpartitions, kafkatools_msg_cb statecb, void *argp, ktproducer_state_t *state);

extern void kafkatools_producer_state_uninit (ktproducer_state_t *state);


/**
 * helper api
 *   read kafka properties and returns count of props
 */
extern int kafkatools_props_readconf (const char *conf_file, const char *section, char **propsbuf, size_t *bufsz);

extern int kafkatools_props_retrieve (const char *propsbuf, size_t bufsz, char **propnames, char **propvalues, int numprops);

extern void kafkatools_propsbuf_free (char *propsbuf);

extern const char * kafkatools_get_rdkafka_version (void);

extern int kafkatools_mutex_lock (pthread_mutex_t * mutex, int is_try);

extern void kafkatools_mutex_unlock (pthread_mutex_t * mutex);


/**
 * kafka producer api
 */
extern int kafkatools_producer_create (char **prop_names, char **prop_values, kafkatools_msg_cb msg_cb, void *msg_opaque, kt_producer *producer);

extern void kafkatools_producer_destroy (kt_producer producer, int flush_wait_ms);

extern const char * kafkatools_producer_get_errstr (kt_producer producer, int *perrcode);

extern rd_kafka_t * kafkatools_producer_get_rdkafka (kt_producer producer);

extern pthread_mutex_t * kafkatools_producer_get_mutex (kt_producer producer);

extern void * kafkatools_producer_get_opaque (kt_producer producer);

extern kt_topic kafkatools_producer_get_topic (kt_producer producer, const char *topic_name);

extern const char * kafkatools_topic_name (const kt_topic topic);

extern int kafkatools_produce_timedwait (kt_producer producer, kafkatools_msg_site_t *ktsite, kafkatools_msg_data_t *ktmsg, int timout_ms, int retry_count);

typedef int(* kt_msgfile_cb)(size_t, void *, size_t, void *);


/**
 * kafka consumer api
 */
typedef int(* kt_tplist_cb)(rd_kafka_topic_partition_t *, void *);

extern int kafkatools_consumer_create (const char *groupid, const char *brokers, int tplist_size, const char * names[], const char * values[], const char * topics[], void * opaque, kt_consumer *outConsumer);

extern void kafkatools_consumer_destroy (kt_consumer consumer);

extern const char * kafkatools_consumer_get_errstr (kt_consumer consumer, int *perrcode);

extern rd_kafka_t * kafkatools_consumer_get_rdkafka (kt_consumer consumer);

extern void * kafkatools_consumer_get_opaque (kt_consumer consumer);

extern pthread_mutex_t * kafkatools_consumer_get_mutex (kt_consumer consumer);

extern rd_kafka_topic_conf_t * kafkatools_consumer_new_topic_conf (kt_consumer consumer, const char * names[], const char * values[]);

extern kt_topic kafkatools_consumer_get_topic (kt_consumer consumer, const char *topic_name, rd_kafka_topic_conf_t *conf);

extern rd_kafka_topic_partition_list_t * kafkatools_consumer_get_topics (kt_consumer consumer);

extern int kafkatools_consumer_start_subscribe (kt_consumer consumer);

extern void kafkatools_list_topic_partitions (rd_kafka_topic_partition_list_t *partitions, kt_tplist_cb tpcb, void *cbarg);

#if defined(__cplusplus)
}
#endif

#endif /* KAFKATOOLS_H_INCLUDED */
