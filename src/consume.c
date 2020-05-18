/**
 * @filename   consume.c
 *  A sample shows how to consume messages from kafka with kafkatools api.
 *
 * @author     Liang Zhang <350137278@qq.com>
 * @version    0.0.9
 * @create     2017-12-20
 * @update     2020-05-22 12:32:50
 */
#include "kafkatools.h"


static int tplist_cb(rd_kafka_topic_partition_t *tp, void *cbarg)
{
    int64_t offlow, offhigh;
    int timeout_ms = 1000;

    rd_kafka_resp_err_t err;

    rd_kafka_t * rk = (rd_kafka_t *) cbarg;

    printf("topic=%s, partition=%d, offset=%ju\n", tp->topic, tp->partition, tp->offset);

    err = rd_kafka_query_watermark_offsets(rk, tp->topic, tp->partition, &offlow, &offhigh, timeout_ms);
    if (err == 0) {
        printf("offlow=%ju, offhigh=%ju\n", offlow, offhigh);
    } else {
        printf("rd_kafka_query_watermark_offsets failed: %s\n", rd_kafka_err2str(err));
    }

    return 1;
}


static void consume_message (rd_kafka_message_t *rkmessage, void *opaque)
{
    if (rkmessage->err) {
        if (rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
            printf("Consumer reached end (topic=%s partition=%d offset=%ju)\n",
                rd_kafka_topic_name(rkmessage->rkt),
                rkmessage->partition,
                rkmessage->offset);
            return;
        }

        if (rkmessage->rkt) {
            printf("Consumer error for (topic=%s partition=%d offset=%ju): %s\n",
                    rd_kafka_topic_name(rkmessage->rkt),
                    rkmessage->partition,
                    rkmessage->offset,
                    rd_kafka_message_errstr(rkmessage));
        } else {
            printf("Consumer error: %s - %s\n",
                    rd_kafka_err2str(rkmessage->err),
                    rd_kafka_message_errstr(rkmessage));
        }

        /*
        if (rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION || rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC) {
            run = 0;
        }

        return;
        */
    }

    if (rkmessage->key_len) {
        printf("Consumer success on (topic=%s partition=%d offset=%ju %zd bytes): msg={%.*s} key=<%.*s>\n",
            rd_kafka_topic_name(rkmessage->rkt),
            rkmessage->partition,
            rkmessage->offset,
            rkmessage->len,
            (int) rkmessage->len, (char *) rkmessage->payload,
            (int) rkmessage->key_len, (char *) rkmessage->key);
    } else {
        printf("Consumer success on (topic=%s partition=%d offset=%ju %zd bytes): {%.*s}\n",
            rd_kafka_topic_name(rkmessage->rkt),
            rkmessage->partition,
            rkmessage->offset,
            rkmessage->len,
            (int) rkmessage->len, (char *) rkmessage->payload);
    }
}


/**
 * Enter the $projectroot/test dir and open 5 terminals as below:
 *
 *   t1) start zookeeper
 *     # ./zookeeper-start.sh
 *
 *   t2) start kafka
 *     # ./kafka-start.sh
 *
 *   t3) run a console-consumer
 *     # ./kafka-consume-topic.sh test
 *
 *   t4) run a console-producer
 *     # kafka/bin/kafka-console-producer.sh --broker-list=localhost:9092 --topic=test --property parse.key=true --property key.separator=:
 *    or
 *     # kafka/bin/kafka-console-producer.sh --broker-list=localhost:9092 --topic=test
 *
 *   t5) send msg no wait
 *     # bin/kafka-console-producer.sh --topic centrum-1-r2p64 --broker-list localhost:9092 --timeout 0
 *
 *   t5) build and run consumer.c test app
 *     # cd $projectroot
 *     # make
 *     # cp libs/lib/librdkafka* src/kafkatools/.libs/
 *     # src/kafkatools/consumer
 *
 * Now we can produce messages within t4) for test!
 */
int main (int argc, char *argv[])
{
    int ret, err;

    kt_consumer consumer;

    const char * names[] = {
        "offset.store.method",
        "auto.offset.reset",
        "enable.partition.eof",
        0
    };

    const char * values[] = {
        "broker",
        "earliest",
        "false",
        0
    };

    const char * topics[] = {
        // "test:0-15",
        // "test2:0-7",

        //"p4:0-3",

        "centrum1r2p4:0-3",
        0
    };

    ret = kafkatools_consumer_create("test_group211", "192.168.39.111:9092", 256, names, values, topics, NULL, &consumer);

    if (ret == KAFKATOOLS_SUCCESS) {
        const char * tpnames[] = {
            "offset.store.method",
            "auto.offset.reset",
            0
        };

        const char * tpvalues[] = {
            "broker",
            "earliest",
            0
        };

        rd_kafka_topic_conf_t * topic_conf = kafkatools_consumer_new_topic_conf(consumer, tpnames, tpvalues);

        if (! topic_conf) {
            printf("kafkatools_consumer_new_topic_conf failed: %s\n", kafkatools_consumer_get_errstr(consumer, 0));
            exit(-1);
        }

        kt_topic rktopic = kafkatools_consumer_get_topic(consumer, "centrum1r2p4", topic_conf);
        if (! rktopic) {
            printf("kafkatools_consumer_get_topic failed: %s\n", kafkatools_consumer_get_errstr(consumer, 0));
            exit(-1);
        }

        rd_kafka_t * rk = kafkatools_consumer_get_rdkafka(consumer);

        printf("kafkatools_consumer_create success.\n");

        rd_kafka_topic_partition_list_t * tplist = kafkatools_consumer_get_topics(consumer);

        kafkatools_list_topic_partitions(tplist, tplist_cb, rk);

        err = rd_kafka_committed(rk, tplist, 5000);
        if (err) {
            printf("rd_kafka_committed failed: %s\n", rd_kafka_err2str(err));
        }

        int partitionid = 1;
        int64_t offsetln = -1;

        err = rd_kafka_consume_start(rktopic, partitionid, offsetln);
        if (err) {
            printf("rd_kafka_consume_start failed: %s\n", rd_kafka_err2str(err));
            exit(0);
        }

        /* 订阅模式: 只能从最新的位置订阅
        err = rd_kafka_subscribe(rk, tplist);
        if (err) {
            printf("rd_kafka_subscribe failed: %s\n", rd_kafka_err2str(err));
        }
        */

        while(1) {
            rd_kafka_message_t *rkmessage;

            /* 订阅模式
             * 轮询消费者的消息或事件, 最多阻塞 timeout_ms.
             * 即使没有预期的消息, 为服务所有排队等待的回调函数, 应用程序
             * 应该定期调用 consumer_poll(), 尤其当注册过 rebalance_cb,
             * 它需要被正确地调用和处理以同步内部消费者状态
            rkmessage = rd_kafka_consumer_poll(rk, 100);
             */

            /* 拉取模式 */
            rd_kafka_poll(rk, 0);

            rkmessage = rd_kafka_consume(rktopic, partitionid, 1 /* wait timeout ms */);
            if (! rkmessage) {
                /* timeout */
                continue;
            }

            consume_message(rkmessage, NULL);

            /**
             * 释放rkmessage的资源, 并把所有权还给 rdkafka
             */
            rd_kafka_message_destroy(rkmessage);
        }

        kafkatools_consumer_destroy(consumer);
    } else {
        printf("kafkatools_consumer_create error.\n");
    }

    return 0;
}
