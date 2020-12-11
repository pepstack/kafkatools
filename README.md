# kafkatools

A C wrapper for librdkafka.

    本项目演示了如何在 C 项目中使用 librdkafka。kafkatools 是一个包装层，简化了一些 librdkafka 的 api 调用。
    但是 kafkatools 并不能屏蔽用户直接调用 librdkafka api。因此 kafkatools.h 引用了：

        #include <rdkafka.h>
        
    作为演示的目的，本项目不提供任何保证。同时也没有对 librdkafka-1.4.2 代码作任何改动。
    但是，kafkatools 已经成功应用在 Windows 和 Linux 项目中。


## Prepare Dependencies for Windows

- librdkafka-1.4.2

[The Apache Kafka C/C++ library](https://github.com/edenhill/librdkafka)

- pthread-w32

[POSIX Threads for Windows](https://sourceforge.net/projects/pthreads4w/files/latest/download)

- zstd-20201211.tar.gz

[Zstandard - Fast real-time compression algorithm](http://www.zstd.net)

[Zstandard - github](https://github.com/facebook/zstd)

  可以用 vs2015 直接打开 deps/zstd/build/VS2015/zstd.sln，编译。


## Build librdkafka with vs2015 for Windows

- Downloads openssl devel libs for Windows to build librdkafka

    $ git clone git@github.com:pepstack/OpenSSL-1.1.1g-Devel-Win.git

- Load 'librdkafka-1.4.2/win32/librdkafkaC.sln' with vs2015

- Config C/C++ include and lib directories to where you put:

    OpenSSL-1.1.1g-Devel-Win

- Build all projects

	librdkafka     - dll
    
	kafkatools     - static lib
	
    consume        - console exe
	
    produce        - console exe

- See dependencies for produce

    $ ldd produce

    You might copy all libkafkatools.so.* into the path which produce depends on.

## Build librdkafka for Linux Server

-- Build Dependencies for Linux

See README.md in ./deps folder.

-- Build libkakfkatools.so

    $ make

Generated libkakfkatools.so will be found in ./target