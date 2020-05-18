#!/bin/bash
#################################################
_file=$(readlink -f $0)
_cdir=$(dirname $_file)
_name=$(basename $_file)


echo "start zookeeper-server..."

export PATH="${_cdir}/kafka/libs:$PATH" && ${_cdir}/kafka/bin/zookeeper-server-start.sh ${_cdir}/kafka/config/zookeeper.properties

