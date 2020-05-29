# Makefile for libkafkatools.so

major_verno = 1
minor_verno = 0
revision_no = 0

# DO NOT change below:

prefix  = .
binname = libkafkatools
version = $(major_verno).$(minor_verno).$(revision_no)
bintarget = $(binname).so.$(version)
binsoname = $(binname).so.$(major_verno)

SRC_DIR = $(prefix)/src
LIB_DIR = $(prefix)/libs/lib

CFLAGS ?= -std=gnu99 -D_GNU_SOURCE -O3 -Wall -pipe -fPIC -DNDEBUG

LDFLAGS ?= -L$(prefix)/target -L$(LIB_DIR) -lrdkafka -lz -lpthread -lrt -lm

INC_DIRS ?= -I$(prefix)/src -I$(prefix)/libs/include

all: $(bintarget)
	-mkdir -p $(prefix)/target
	-cp $(bintarget) $(prefix)/target/
	-ln -sf $(bintarget) $(prefix)/target/$(binsoname)
	-ln -sf $(bintarget) $(prefix)/target/$(binname).so


$(bintarget): kafkatools_consumer.o kafkatools_producer.o red_black_tree.o readconf.o misc.o
	$(CC) $(CFLAGS) -shared \
		-Wl,--soname=$(binsoname) \
		-Wl,--rpath='$(prefix):$(prefix)/lib:$(prefix)/libs/lib' \
		-o $@ \
		*.o \
		$(LDFLAGS)

kafkatools_consumer.o: $(SRC_DIR)/kafkatools_consumer.c
	$(CC) $(CFLAGS) $(INC_DIRS) -c $(SRC_DIR)/kafkatools_consumer.c -o $@

kafkatools_producer.o: $(SRC_DIR)/kafkatools_producer.c
	$(CC) $(CFLAGS) $(INC_DIRS) -c $(SRC_DIR)/kafkatools_producer.c -o $@

red_black_tree.o: $(SRC_DIR)/common/red_black_tree.c
	$(CC) $(CFLAGS) $(INC_DIRS) -c $(SRC_DIR)/common/red_black_tree.c -o $@

readconf.o: $(SRC_DIR)/common/readconf.c
	$(CC) $(CFLAGS) $(INC_DIRS) -c $(SRC_DIR)/common/readconf.c -o $@

misc.o: $(SRC_DIR)/common/misc.c
	$(CC) $(CFLAGS) $(INC_DIRS) -c $(SRC_DIR)/common/misc.c -o $@


# produce test app
produce: $(bintarget) $(SRC_DIR)/produce.c
	$(CC) $(CFLAGS) $(INC_DIRS) $(LDFLAGS) $(SRC_DIR)/produce.c -o $@ \
	-lkafkatools
	-cp $(prefix)/produce $(prefix)/target/
	-cp $(prefix)/kafka-producer.properties $(prefix)/target/


clean:
	-rm -f $(prefix)/*.o
	-rm -f $(prefix)/$(bintarget)
	-rm -f $(prefix)/$(binname).so
	-rm -f $(prefix)/produce
	-rm -f $(prefix)/consume
	-rm -f $(prefix)/target/*


.PHONY: all

