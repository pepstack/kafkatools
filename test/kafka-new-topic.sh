#!/bin/bash
#################################################
_file=$(readlink -f $0)
_cdir=$(dirname $_file)
_name=$(basename $_file)

# java-1.8 and later

if [ "$#" = "0" ]; then
    echo "no topic specifiedi !" 
    echo "$_name topic"
    exit 1
fi

echo "kafka create topic: topic=$1"

export PATH="${_cdir}/kafka/libs:$PATH" && ${_cdir}/kafka/bin/kafka-topics.sh --zookeeper localhost:2181 --create --topic="$1" --replication-factor 1 --partitions 8

