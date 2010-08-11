#
# Makefile for DTrace
#

obj-$(CONFIG_DTRACE)		+= dtrace.o
obj-$(CONFIG_DT_FASTTRAP)	+= fasttrap.o
obj-$(CONFIG_DT_FBT)		+= fbt.o
obj-$(CONFIG_DT_LOCKSTAT)	+= lockstat.o
obj-$(CONFIG_DT_PROFILE)	+= profile.o
obj-$(CONFIG_DT_SDT)		+= sdt.o
obj-$(CONFIG_DT_SYSTRACE)	+= systrace.o

dtrace-y			:= dtrace_mod.o dtrace_dev.o \
				   dtrace_ptofapi.o
fasttrap-y			:= fasttrap_mod.o fasttrap_dev.o
fbt-y				:= fbt_mod.o fbt_dev.o
lockstat-y			:= lockstat_mod.o lockstat_dev.o
profile-y			:= profile_mod.o profile_dev.o
sdt-y				:= sdt_mod.o sdt_dev.o
systrace-y			:= systrace_mod.o systrace_dev.o
