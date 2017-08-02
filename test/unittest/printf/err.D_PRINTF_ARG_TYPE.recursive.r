-- @@stderr --
dtrace: failed to compile script test/unittest/printf/err.D_PRINTF_ARG_TYPE.recursive.d: [D_PRINTF_ARG_TYPE] line 21: printf( ) argument #2 is incompatible with conversion #1 prototype:
	conversion: %s
	 prototype: char [] or string (or use stringof)
	  argument: void
