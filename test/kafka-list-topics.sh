#!/bin/bash
#################################################
_file=$(readlink -f $0)
_cdir=$(dirname $_file)
_name=$(basename $_file)

# java-1.8 and later

echo "list all kafka topics..."

export PATH="${_cdir}/kafka/libs:$PATH" && ${_cdir}/kafka/bin/kafka-topics.sh --zookeeper=localhost:2181 --list

