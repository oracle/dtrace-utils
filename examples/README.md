## Synopsis

This is the README file for the examples directory that is included in the
DTrace distribution directory.  These examples are meant to be learned
from.  They can also be a starting point to explore changes and enhancements
you'd like to make to these scripts.

## Target audience

The scripts target a wide range of users.  There are examples that are
ideally suited for the beginners, but there also scripts that are
probably easier to understand once you have some more practical experiences.
To make it easier to select a script suitable for your level of experience,
we have added a level indicator, but please do not let that withold you from
looking at other examples.

| Level | Description |
| :---: | :----       |
| B     | Beginner - Only some basic knowledge of DTrace is assumed. |
| M	| Medium  - Some experience with DTrace will be helpful.     |
| A     | Advanced - Practical experiences writing scripts is recommended. |

## Installation

There is no need for any additional installation instructions.  Use the
examples as provided, or make changes as you see fit.  You can do so
in-place, or get a copy in your working directory.

## Usage

Each script has a usage line that you can find under SYNOPSIS in the
top part of the script.  There are also extensive additional comments.

In the comments, we often refer to a target application, or command.
With the latter we mean a Linux command.
Most scripts can be used for either, and also work on a running process.
Please check the comments though, because some scripts assume there is
a program called main, for example.

The command given in the SYNOPSIS section may need to be modified
according to what needs to be traced:

| Option        | Description |
| :---          | :----       |
| `-c <command>`| Trace the command (or application).  Tracing ends when the      |
|               | command/application terminates.                                 |
| `-p <pid>`	| Trace the process with id \<pid>.  Stop the tracing with ctrl-C. |

An example of a script that can be used for both is `sched-simple.d`:

```
$ sudo ./sched-simple.d -c ls
$ sudo ./sched-simple.d -p <pid-of-target>
ctrl-C
```

## The examples

These are the scripts included, plus a brief description.

| Script          | Description                | Level |
| --------------- | -------------------------- | :---: |
| fcalls.d        | List the functions executed by the target application.  |B|
|                 | In addition to this, the number of calls to each function |
|                 | is printed.  This information is given on a per-thread    |
|                 | basis, as well as aggregated over all threads.            |
| io-cast-net.d   | For the target application, show a list of all the libc |B|
|                 | functions that include one of the selected keywords       |
|                 | (open,close,read,write) in their name.
| io-stats.d      | Show statistics for the fopen(), fread(), fwrite(),     |A|
|                 | pwrite() and read() functions.  There are several probes  |
|                 | and you are encouraged to experiment with this script.    |
| sched-simple.d  | Use the sched provider to count how often the target    |B|
|                 | application, or command, was scheduled on and off the     |
|                 | CPU.  The information is given for all individual threads,|
|                 | as well as across all the threads.                        |
| sched-stats.d   | Display various scheduler statistics for the target     |A|
|                 | application, or command.  For each thread, a histogram of |
|                 | the time on the CPU is shown.  Plus global statistics,    |
|                 | like the total time on the CPU, and the minimum and       |
|                 | maximum.  Those threads with an on-CPU time below a user  |
|                 | controllable limit are listed.  Also the CPU IDs and the  |
|                 | number of times a thread was on/off CPU are given.        |
| thread-ids.d    | For a Pthreads application, show the mapping between the|M|
|                 | thread IDs as created by the Pthreads system and the      |
|                 | thread ID as returned in the DTrace tid variable.         |
| var-scope.d     | Demonstrate some of the scoping rules for global and    |B|
|                 | clause-local variables.                                   |
