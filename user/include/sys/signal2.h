#ifndef _S_SYS_SIGNAL2_H
#define	_S_SYS_SIGNAL2_H

#include <types_various.h>
#include <sys/types32.h>
#include <sys/regset.h>
#include <sys/ucontext.h>
#include <signal.h>

#define	sa_handler32	_funcptr._handler
#define	sa_sigaction32	_funcptr._sigaction


#define	SIGEMT		65	/* EMT instruction */
#define	SI_NOINFO	32767	/* no signal information */


union sigval32 {
        int32_t sival_int;      /* integer value */
        caddr32_t sival_ptr;    /* pointer value */
};

/* X/Open requires some more fields with fixed names.  */
# undef si_pid
# undef si_uid
# undef si_timerid
# undef si_overrun
# undef si_status
# undef si_utime
# undef si_stime
# undef si_value
# undef si_int
# undef si_ptr
# undef si_addr
# undef si_band 
# undef si_fd


typedef struct siginfo32 {

        int32_t si_signo;                       /* signal from signal.h */
        int32_t si_code;                        /* code from above      */
        int32_t si_errno;                       /* error from errno.h   */

      union
	{
	 
                int32_t _pad[__SI_PAD_SIZE];   /* for future growth    */

        struct
	   {          /* kill(), SIGCLD, siqqueue() */
                        pid32_t si_pid;
			uid32_t si_uid;		/* Real user ID of sending process.  */
           } _kill;	
	/* POSIX.1b timers.  */
        struct
          {
            int si_tid;         /* Timer ID.  */
            int si_overrun;     /* Overrun count.  */
            sigval_t si_sigval; /* Signal value.  */
          } _timer;

        /* POSIX.1b signals.  */
        struct
          {
            pid32_t si_pid;     /* Sending process ID.  */
            uid32_t si_uid;     /* Real user ID of sending process.  */
            sigval_t si_sigval; /* Signal value.  */
          } _rt;

        /* SIGCHLD.  */
        struct
          {
            pid32_t si_pid;     /* Which child */
            uid32_t si_uid;     /* Real user ID of sending process.  */
            int si_status;      /* Exit value or signal.  */
            __clock_t si_utime;
            __clock_t si_stime;
          } _sigchld;

        /* SIGILL, SIGFPE, SIGSEGV, SIGBUS.  */
        struct
          {
            void *si_addr;      /* Faulting insn/memory ref  */
          } _sigfault;

        /* SIGPOLL.  */
        struct
          {
            long int si_band;   /* Band event for SIGPOLL */
            int si_fd;
          } _sigpoll;
      } _sifields;
} siginfo32_t;

/* X/Open requires some more fields with fixed names.  */
# define si_pid         _sifields._kill.si_pid
# define si_uid         _sifields._kill.si_uid
# define si_timerid     _sifields._timer.si_tid
# define si_overrun     _sifields._timer.si_overrun
# define si_status      _sifields._sigchld.si_status
# define si_utime       _sifields._sigchld.si_utime
# define si_stime       _sifields._sigchld.si_stime
# define si_value       _sifields._rt.si_sigval
# define si_int         _sifields._rt.si_sigval.sival_int
# define si_ptr         _sifields._rt.si_sigval.sival_ptr
# define si_addr        _sifields._sigfault.si_addr
# define si_band        _sifields._sigpoll.si_band
# define si_fd          _sifields._sigpoll.si_fd


struct sigaction32 {
	int32_t		sa_flags;
	union {
		caddr32_t	_handler;
		caddr32_t	_sigaction;
	}	_funcptr;
	sigset_t	sa_mask;
	int32_t		sa_resv[2];
};

/* Duplicated in <sys/ucontext.h> as a result of XPG4v2 requirements. */
#ifndef	_STACK_T
#define	_STACK_T

/* Kernel view of the ILP32 user sigaltstack structure */

typedef struct sigaltstack32 {
	caddr32_t	ss_sp;
	size32_t	ss_size;
	int32_t		ss_flags;
} stack32_t;


#endif /* _STACK_T */

typedef struct {
	gregset32_t	gregs;		/* general register set */
	fpregset32_t	fpregs;		/* floating point register set */
	int32_t oldmask;
    	int32_t cr2;
} mcontext32_t;


/* Userlevel context.  */
typedef struct ucontext32
  {
    uint32_t uc_flags;
    caddr32_t uc_link;
    stack32_t uc_stack;
    mcontext32_t uc_mcontext;
    __sigset_t uc_sigmask;
    struct _libc_fpstate __fpregs_mem;
  } ucontext32_t;




#endif /* _SYS_SIGNAL_H */
