# kafkatools

A C wrapper for librdkafka.

## Dependencies

librdkafka-1.4.2


## Build librdkafka with vs2015

- Downloads openssl devel for Windows Libs to build librdkafka

    git@github.com:pepstack/OpenSSL-1.1.1g-Devel-Win.git

- Load 'librdkafka-1.4.2/win32/librdkafka.sln' with vs2015

- Config C/C++ include and lib directories to where you put:

    OpenSSL-1.1.1g-Devel-Win

- Build only librdkafka project


## Build kafkatools with vs2015

- Load 'win32/kafkatools.sln' with vs2015

- Build kafkatools project