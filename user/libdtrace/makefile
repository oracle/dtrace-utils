BUILD_DIR=build-$(shell uname -r)
BINDIR = ../$(BUILD_DIR)
CC=/usr/bin/gcc -g $(BUILD_BITS)
COMPILE.cpp= $(CC) -E -C $(CFLAGS) $(CPPFLAGS)
MACH = $(shell uname -i)
CPPFLAGS += -I. \
	-I../common/ctf \
	-I../uts/common/ \
	-I../include \
	-I../libproc/common \
	-I../libctf \
	-I../uts/intel \
	-I$(BINDIR) \
	-DCTF_OLD_VERSIONS $(PTR32)
CPPFLAGS += -D_ILP32 -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 -D_LONGLONG_TYPE

LIB = ../$(BUILD_DIR)/libdtrace.a
DLIBDIR = /usr/lib/dtrace

$(LIB): \
	$(LIB)(drti.o) \
	$(LIB)(dt_lex.o) \
	$(LIB)(dt_aggregate.o) \
	$(LIB)(dt_as.o) \
	$(LIB)(dt_buf.o) \
	$(LIB)(dt_cc.o) \
	$(LIB)(dt_cg.o) \
	$(LIB)(../driver/dis_tables.o) \
	$(LIB)(dt_isadep.o) \
	$(LIB)(dt_consume.o) \
	$(LIB)(dt_decl.o) \
	$(LIB)(dt_dis.o) \
	$(LIB)(dt_dof.o) \
	$(LIB)(dt_error.o) \
	$(LIB)(dt_errtags.o) \
	$(LIB)(dt_grammar.o) \
	$(LIB)(dt_handle.o) \
	$(LIB)(dt_ident.o) \
	$(LIB)(dt_inttab.o) \
	$(LIB)(dt_link.o) \
	$(LIB)(dt_list.o) \
	$(LIB)(dt_map.o) \
	$(LIB)(dt_module.o) \
	$(LIB)(dt_names.o) \
	$(LIB)(dt_open.o) \
	$(LIB)(dt_options.o) \
	$(LIB)(dt_parser.o) \
	$(LIB)(dt_pcb.o) \
	$(LIB)(dt_pid.o) \
	$(LIB)(dt_pragma.o) \
	$(LIB)(dt_printf.o) \
	$(LIB)(dt_proc.o) \
	$(LIB)(dt_program.o) \
	$(LIB)(dt_provider.o) \
	$(LIB)(dt_regset.o) \
	$(LIB)(dt_string.o) \
	$(LIB)(dt_strtab.o) \
	$(LIB)(dt_subr.o) \
	$(LIB)(dt_work.o) \
	$(LIB)(dt_xlator.o)

DLIBSRCS += \
        errno.d \
        net.d \
        fc.d \
        ip.d \
        iscsit.d \
        nfs.d \
        procfs.d \
        regs.d \
        sched.d \
        signal.d \
        scsi.d \
        srp.d \
        sysevent.d \
        tcp.d \
        udp.d \
        unistd.d

$(BINDIR)/dt_grammar.h $(LIB)(dt_grammar.o): dt_grammar.y $(H)
	$(YACC) -d dt_grammar.y
	mv y.tab.h $(BINDIR)/dt_grammar.h
	mv y.tab.c dt_grammar.c
	$(CC) -DYYDEBUG=1 -DYYERROR_VERBOSE $(CPPFLAGS) -c dt_grammar.c
	ar rv $(LIB) dt_grammar.o
	-rm -f dt_grammar.c dt_grammar.o

$(LIB)(dt_lex.o): $(BINDIR)/dt_grammar.h $(BINDIR)/dt_lex.c $(H)
	 $(CC) $(CPPFLAGS) -c $(BINDIR)/dt_lex.c
	 ar rv $(LIB) dt_lex.o
	 -rm -f dt_lex.o
$(BINDIR)/dt_lex.c: dt_lex.l
	lex -l -t -v dt_lex.l > $(BINDIR)/dt_lex.c


$(LIB)(dis_tables.o): ../driver/dis_tables.c $(H)
	$(CC) $(CPPFLAGS) -Ii386 -I../driver -c ../driver/dis_tables.c
	ar rv $(LIB) dis_tables.o
	-rm -f dis_tables.o

$(LIB)(dt_isadep.o): ./i386/dt_isadep.c $(H)
	$(CC) $(CPPFLAGS) -Ii386 -I../driver -c ./i386/dt_isadep.c
	ar rv $(LIB) dt_isadep.o
	-rm -f dt_isadep.o

$(LIB)(dt_module.o): dt_module.c $(H)
	$(CC) -I. -I../common/ctf \
        -I../uts/common/ \
        -I../include \
        -I../libproc/common \
        -I../libctf \
        -I../uts/intel \
        -I$(BINDIR) \
        -DCTF_OLD_VERSIONS $(PTR32) \
	-D_ILP32 -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -D_LONGLONG_TYPE \
	-c dt_module.c
	ar rv $(LIB) dt_module.o
	-rm -f dt_module.o

dt_errtags.c: dt_errtags.h mkerrtags.sh
	sh mkerrtags.sh <dt_errtags.h | sed -e 's/\\n/\n/g' > dt_errtags.c

dt_names.c: mknames.sh
	sh mknames.sh <../uts/common/sys/dtrace.h | sed -e 's/\\n/\n/g' > dt_names.c

%.sed.in.c: %.sed.in
	cp $< $@
%.sed: %.sed.in.c
	$(COMPILE.cpp) -D_KERNEL $< | tr -d ' ' | tr '"' '@' | \
            sed 's/\&/\\\&/g' | grep '^s/' > $@
	-rm -f $<

errno.d: mkerrno.sh /usr/include/asm-generic/errno-base.h
	sh mkerrno.sh < /usr/include/asm-generic/errno-base.h \
	| sed -e 's/\\n/\n/g' > $@

signal.d: mksignal.sh /usr/include/bits/signum.h
	sh mksignal.sh < /usr/include/bits/signum.h \
	| sed -e 's/\\n/\n/g' \
	| sed -e '/inline int SIGCLD/{h;d}' -e '/"1.0" SIGCHLD/{G;}' \
	| sed -e '/"1.0" SIGCLD/{h;d}' -e '/ = SIGCHLD;/{G;}' \
	| sed -e '/inline int SIGPOLL/{h;d}' -e '/"1.0" SIGIO/{G;}' \
	| sed -e '/"1.0" SIGPOLL/{h;d}' -e '/ = SIGIO/{G;}' \
	| sed -e '/SIGRTMIN/d' -e '/SIGRTMAX/d' > $@


procfs.d: procfs.sed procfs.d.in
	sed -f procfs.sed < procfs.d.in > $@
io.d: io.sed io.d.in
	sed -f io.sed < io.d.in > $@
ip.d: ip.sed ip.d.in
	sed -f ip.sed < ip.d.in > $@
net.d: net.sed net.d.in
	sed -f net.sed < net.d.in > $@
sysevent.d: sysevent.sed sysevent.d.in
	sed -f sysevent.sed < sysevent.d.in > $@
tcp.d: tcp.sed tcp.d.in
	sed -f tcp.sed < tcp.d.in > $@
udp.d: udp.sed udp.d.in
	sed -f udp.sed < udp.d.in > $@

./i386/regs.sed: ./i386/regs.sed.in
	cp ./i386/regs.sed.in ./i386/regs.sed.in.c
	$(COMPILE.cpp) -D_KERNEL ./i386/regs.sed.in.c | tr -d ' ' | tr '"' '@' | \
            sed 's/\&/\\\&/g' | grep '^s/' > $@
	-rm ./i386/regs.sed.in.c
regs.d: ./i386/regs.sed ./i386/regs.d.in
	sed -f ./i386/regs.sed < ./i386/regs.d.in > $@
	-rm ./i386/regs.sed

clean:
	-rm -f dt_grammar.h dt_lex.c dt_errtags.c dt_names.c *.o $(LIB) 
	-rm -f errno.d signal.d regs.d procfs.d io.d ip.d net.d sysevent.d tcp.d udp.d
	-rm -f *.sed i386/*.sed
uninstall:
	-rm -rf $(DLIBDIR)

all: $(LIB) $(DLIBSRCS)
	@/bin/true

install:
	if [ ! -d $(DLIBDIR) ] ; then \
		mkdir $(DLIBDIR) ; \
	fi
	install -m 755 -o root errno.d regs.d signal.d \
	unistd.d $(DLIBDIR)


