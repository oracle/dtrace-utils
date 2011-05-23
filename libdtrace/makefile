BINDIR = ../$(BUILD_DIR)

CPPFLAGS += -I. \
	-I../common/ctf \
	-I../uts/common/ \
	-I../include \
	-I../libproc/common \
	-I../libctf \
	-I../uts/intel \
	-I$(BINDIR) \
	-DCTF_OLD_VERSIONS $(DTO)

LIB = ../$(BUILD_DIR)/libdtrace.a
DRTI_OBJ = ../$(BUILD_DIR)/drti.o
DLIBDIR = /usr/lib/dtrace

CPPFLAGS += -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 -D_LONGLONG_TYPE -DDTRACE_LIBDIR="\"$(DLIBDIR)\""

$(LIB): \
	$(LIB)(dt_lex.o) \
	$(LIB)(dt_aggregate.o) \
	$(LIB)(dt_as.o) \
	$(LIB)(dt_buf.o) \
	$(LIB)(dt_cc.o) \
	$(LIB)(dt_cg.o) \
	$(LIB)(dis_tables.o) \
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

SED_DLIBS += \
	procfs.d \
	io.d \
	ip.d \
	net.d \
	sysevent.d \
	tcp.d \
	udp.d

BUILD_DLIBS += \
	$(SED_DLIBS) \
	errno.d \
	signal.d \
	regs.d

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
	flex -v -o $(BINDIR)/dt_lex.c dt_lex.l

$(LIB)(dis_tables.o): ../common/dis/dis_tables.c $(H)
	$(CC) $(CPPFLAGS) -Ii386 -I../common/dis/ -c ../common/dis/dis_tables.c
	ar rv $(LIB) dis_tables.o
	-rm -f dis_tables.o

$(LIB)(dt_isadep.o): ./i386/dt_isadep.c $(H)
	$(CC) $(CPPFLAGS) -Ii386 -I../common/dis/ -c ./i386/dt_isadep.c
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
        -DCTF_OLD_VERSIONS $(DTO) \
	-D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -D_LONGLONG_TYPE \
	-c dt_module.c
	ar rv $(LIB) dt_module.o
	-rm -f dt_module.o

$(DRTI_OBJ): drti.o
	cp $< $@

dt_errtags.c: dt_errtags.h mkerrtags.sh
	sh mkerrtags.sh <dt_errtags.h | sed -e 's/\\n/\n/g' > dt_errtags.c

dt_names.c: mknames.sh
	sh mknames.sh <../uts/common/sys/dtrace.h | sed -e 's/\\n/\n/g' > dt_names.c

# Normal substitutions.

%.sed: %.sed.in
	$(COMPILE.cpp) -x c -D_KERNEL $< | tr -d ' ' | tr '"' '@' | \
            sed 's/\&/\\\&/g' | grep '^s/' > $@

$(SED_DLIBS): %.d: %.sed %.d.in
	sed -f $*.sed < $*.d.in > $@

# Custom substitutions.

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

./i386/regs.sed: ./i386/regs.sed.in
	$(COMPILE.cpp) -x c -I../include -D_KERNEL ./i386/regs.sed.in | tr -d ' ' | tr '"' '@' | \
            sed 's/\&/\\\&/g' | grep '^s/' > $@
regs.d: ./i386/regs.sed ./i386/regs.d.in
	sed -f ./i386/regs.sed < ./i386/regs.d.in > $@
	-rm ./i386/regs.sed

clean:
	-rm -f dt_grammar.h dt_lex.c dt_errtags.c dt_names.c *.o $(LIB) 
	-rm -f $(BUILD_DLIBS)
	-rm -f *.sed i386/*.sed
uninstall:
	-rm -rf $(DLIBDIR)

all: $(LIB) $(DLIBSRCS) $(DRTI_OBJ)
	@/bin/true

install:
	mkdir -p $(DLIBDIR)
	install -m 644 -o root -g root drti.o $(DLIBDIR)
	install -m 644 -o root -g root errno.d regs.d signal.d unistd.d \
		drti-vers $(DLIBDIR)


