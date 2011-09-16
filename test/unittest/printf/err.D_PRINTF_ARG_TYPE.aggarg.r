-- @@stderr --
dtrace: failed to compile script test/unittest/printf/err.D_PRINTF_ARG_TYPE.aggarg.d: [D_PRINTF_ARG_TYPE] line 41: printf( ) argument #3 is incompatible with conversion #2 prototype:
	conversion: %d
	 prototype: char, short, int, long, or long long
	  argument: aggregation
