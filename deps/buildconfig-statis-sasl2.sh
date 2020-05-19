# This config shell file used to build librdkafka.so (static link to libsasl2.a)
#  and install it in your specified prefix path.
#
# This file must be found in librdkafka folder together with configure.
#
# YOU MUST REPLACE "/path/to/" BY YOUR PATH IN MY CASE IS:
#    "/root/Workspace/github.com"
#
# Usage:
#   $ cd librdkafka/
#   $ ./buildconfig-static-sasl2.sh
#   $ make && sudo make install
#
STATIC_LIB_zlib=/path/to/kafkatools/libs/lib/libz.a \
STATIC_LIB_libcrypto=/path/to/kafkatools/libs/lib/libcrypto.a \
STATIC_LIB_libssl=/path/to/kafkatools/libs/lib/libssl.a \
STATIC_LIB_libsasl2=/path/to/kafkatools/libs/lib/libsasl2.a \
CFLAGS="-fPIC" ./configure --prefix=/path/to/kafkatools/libs \
--enable-zlib \
--enable-ssl \
--enable-sasl \
--enable-gssapi \
--disable-syslog
