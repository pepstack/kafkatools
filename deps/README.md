# Build Dependencies for librdkafka On Linux Server

OS: rhel6.4

GCC: 6.4.0

    If not gcc-6.x, you should upgrade gcc/g++ first. see also:
    
    [UPDATE GCC On RHEL6 (x64)](https://github.com/pepstack/update-gcc-el6)

    If you are using rhel7.x or others with GCC 6.x+, Please ignore above notes.

GNU Make 3.81


!!! PLEASE REPLACE "/path/to/" BY YOUR PATH IN MY CASE IS: "/root/Workspace/github.com"


## zlib-1.2.11

    $ cd zlib-1.2.11/

    $ CFLAGS="-fPIC" ./configure --prefix=/path/to/kafkatools/libs --static

    $ make && sudo make install


## openssl-1.1.1g

    $ cd openssl-1.1.1g/

    $ ./config --prefix=/path/to/kafkatools/libs

    $ make && make install


## cyrus-sasl-2.1.27

    $ cd cyrus-sasl-2.1.27/

    $ CFLAGS="-fPIC" ./configure --prefix=/path/to/kafkatools/libs --enable-static

    $ make && sudo make install


## zstd

    $ git clone git@github.com:facebook/zstd.git

    TODO:

        how to build


## librdkafka-1.4.2

Create a build config shell script file under librdkafka-1.4.2 folder as below:

```
    # buildconfig.sh
    #   configure how to build librdkafka
    STATIC_LIB_zlib=/path/to/kafkatools/libs/lib/libz.a \
    STATIC_LIB_libcrypto=/path/to/kafkatools/libs/lib/libcrypto.a \
    STATIC_LIB_libssl=/path/to/kafkatools/libs/lib/libssl.a \
    ./configure --prefix=/path/to/kafkatools/libs \
    --enable-zlib \
    --enable-ssl \
    --enable-gssapi \
    --disable-syslog
```

Then build and install librdkafka:

    $ ./buildconfig.sh

    $ make && make install
