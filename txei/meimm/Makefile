PACKAGE := meimm
INCLUDES := -Iinclude -I.
CFLAGS += -Wall -ggdb -I. -fPIC -O2 $(INCLUDES)
MEIMM := meimm
LIBS := lib$(MEIMM).so

all: $(LIBS) meimm-test

clean:
	$(RM) *.o meimm

meimm-test.c: meimm.h

meimm-test: meimm-test.o $(LIBS)
	$(CC) $(LDFLAGS) -o $@ meimm-test.o  -Wl,-rpath=. -L.  -lmeimm

lib%.so: %.o
	$(CC) $(LDFLAGS) --shared $^ -o $@

pack: ver=$(shell git describe)
pack:
	git archive --format=tar --prefix=$(PACKAGE)-$(ver)/ HEAD | gzip > $(PACKAGE)-$(ver).tar.gz
