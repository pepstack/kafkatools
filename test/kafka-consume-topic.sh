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

echo "kafka console consume: topic=$1"

export PATH="${_cdir}/kafka/libs:$PATH" && ${_cdir}/kafka/bin/kafka-console-consumer.sh --bootstrap-server=192.168.39.111:9092 --topic="$1" --property print.key=true

