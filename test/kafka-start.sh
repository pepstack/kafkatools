#!/bin/bash
#################################################
_file=$(readlink -f $0)
_cdir=$(dirname $_file)
_name=$(basename $_file)

# java-1.8 and later

echo "start kafka-server..."

export PATH="${_cdir}/kafka/libs:$PATH" && ${_cdir}/kafka/bin/kafka-server-start.sh ${_cdir}/kafka/config/server.properties

