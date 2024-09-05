<!--
 SPDX-FileCopyrightText: 2013,2024 Oracle and/or its affiliates.
 SPDX-License-Identifier: CC-BY-SA-4.0
-->

---
publisherinformation: September 2022
---

# Tracing User-Space Applications

This chapter provides information about how to trace a user-space application and includes examples of D programs that you can use to investigate what is happening in an example user-space program.

## Preparing for Tracing User-Space Applications

The DTrace helper device \(`/dev/dtrace/helper`\) enables a user-space application that contains DTrace probes to send probe provider information to DTrace.

To trace user-space processes that are run by users other than `root`, you must change the mode of the DTrace helper device to allow the user to record tracing information, as shown in the following examples.

### Example: Changing the Mode of the DTrace Helper Device

The following example shows how you would enable the tracing of user-space applications by users other than the `root` user.

```
# `chmod 666 /dev/dtrace/helper`
```

Alternatively, if the `acl` package is installed on your system, you would use an ACL rule to limit access to a specific user, for example:

```
# `setfacl -m u:guest:rw /dev/dtrace/helper`
```

**Note:**

For DTrace to reference the probe points, you must change the mode on the device before the user begins running the program.

You can also create a `udev` rules file such as `/etc/udev/rules.d/10-dtrace.rules` to change the permissions on the device file each time the system boots.

The following example shows how you would change the mode of the device file by adding the following line to the `udev` rules file:

```
kernel=="dtrace/helper", MODE="0666"
```

The following example shows how you would change the ACL settings for the device file by adding a line similar to the following to the `udev` rules file:

```
kernel=="dtrace/helper", RUN="/usr/bin/setfacl -m u:guest:rw /dev/dtrace/helper"
```

To apply the `udev` rule without needing to restart the system, you would run the **start\_udev** command.

## Sample Application

This section provides a sample application to be used in subsequent exercises and examples in this chapter. The example, which illustrates a simple program, favors brevity and probing opportunity rather than completeness or efficiency.

**Note:**

The following simple program is provided for example purposes *only* and is not intended to efficiently solve a practical problem nor exhibit preferred coding methods.

The sample program finds the lowest factor of a number, which you input. The program is comprised of the following four files: `makefile`, `primelib.h`, `primelib.c`, and `primain.c` , which are stored in the same working directory.

### Description and Format of the makefile File

The following example shows the contents of the `makefile` file.

**Note:**

A `makefile` must use tabs for indentation so that the `make` command can function properly. Also, be sure that tabs are retained if the file is copied and then used.

```
default: prime

# compile the library primelib first
primelib.o: primelib.c
	gcc -c primelib.c

# compile the main program
primain.o: primain.c
	gcc -c primain.c

# link and create executable file "prime"
prime: primelib.o primain.o
	gcc primain.o primelib.o -o prime -lm

clean:
	-rm -f *.o
	-rm -f prime
```

### Description of the primelib.h Source File

The following example shows the contents of the `primelib.h` file.

```
int findMaxCheck( int inValue );
int seekFactorA( int input, int maxtry );
int seekFactorB( int input );
```

### Description of the primelib.c Source File

The following example shows the contents of the `primelib.c` file.

```
#include <stdio.h>
#include <math.h>

/*
 * utility functions which are called from the main source code
 */

// Find and return our highest value to check -- which is the square root
int findMaxCheck( int inValue )  {
  float sqRoot;
  sqRoot = sqrt( inValue );
  printf("Square root of %d is %lf\n", inValue, sqRoot); 
  return floor( sqRoot );
  return inValue/2;
}

int debugFlag = 0;

// Search for a factor to the input value, proving prime on return of zero
int seekFactorA( int input, int maxtry )  {
  int divisor, factor = 0;
  for( divisor=2; divisor<=maxtry; ++divisor ) {
    if( 0 == input%divisor ) {
      factor = divisor;
      break;
    }
    else if ( debugFlag != 0 )
      printf( "modulo %d yields: %d\n", divisor, input%divisor );
  }
  return factor;
}

// Search for a factor to the input value, proving prime on return of zero
// This is a different method than "A", using one argument
int seekFactorB( int input )  {
  int divisor, factor = 0;
  if( 0 == input%2 ) return 2;
  for( divisor=3; divisor<=input/2; divisor+=2 ) {
    if( 0 == input%divisor ) {
      factor = divisor;
      break;
    }
  }
  return factor;
}
```

### Description of the primain.c Source File

The following example shows the contents of the `primain.c` file.

```
#include <stdio.h>
#include "primelib.h"

/*
 * Nominal C program churning to provide a code base we might want to
 * instrument with D
*/

// Search for a divisor -- thereby proving composite value of the input.
int main()  {
  int targVal, divisor, factorA=0, factorB=0;

  printf( "Enter a positive target integer to test for prime status: " );
  scanf( "%d", &targVal );

  // Check that the user input is valid
  if( targVal < 2 ) {
    printf( "Invalid input value -- exiting now\n" );
    return -2;
  }

  // Search for a divisor using method and function A
  int lastCheck;
  lastCheck = findMaxCheck( targVal );
  printf( "%d highest value to check as divisor\n", lastCheck );
  factorA = seekFactorA( targVal, lastCheck );

  // Search for a divisor using method and function B
  factorB = seekFactorB( targVal );

  // Warn if the methods give different results
  if (factorA != factorB)
    printf( "%d does not equal %d! How can this be?\n", factorA, factorB );

  // Print results
  if( !factorA )
    printf( "%d is a prime number\n", targVal );
  else
    printf( "%d is not prime because there is a factor %d\n",
	    targVal, factorA );
  return 0;
}
```

### Compiling the Program and Running the prime Executable

With the four files previously described located in the same working directory, compile the program by using the `make` command as follows:

```
# `make`
gcc -c primelib.c
gcc -c primain.c
gcc primain.o primelib.o -o prime -lm
```

Running the `make` command creates an executable named `prime`, which can be run to find the lowest prime value of the input, as shown in the following two examples:

```
# `./prime `
Enter a positive target integer to test for prime status: 5099
Square root of 5099 is 71.407280
71 highest value to check as divisor
5099 is a prime number
```

```
# `./prime`
Enter a positive target integer to test for prime status: 95099
Square root of 95099 is 308.381256
308 highest value to check as divisor
95099 is not prime because there is a factor 61
```

After compiling the program and running the `prime` executable, you can practice adding USDT probes to an application, as described in [Adding USDT Probes to an Application](dtrace-tutorial-TracingUserSpaceApplications.md#).

## Adding USDT Probes to an Application

In this section, we practice adding USDT probes to an application. 

To get started, you will need to create a `.d` file.

**Note:**

This `.d` file is not a script that is run in the same way that is shown in previous examples in this tutorial, but is rather the `.d` source file that you use when compiling and linking your application. To avoid any confusion, use a different naming convention for this file than you use for scripts.

After creating the `.d` file, you then need to create the required probe points to use in the following examples. This information is added to the `primain.c` source file. The probe points that are used in this practice are those listed in the following table. These probes represent a sequence of operations and are used after the user entry is completed and checked.

<table><thead><tr><th>

Description

</th><th>

Probe

</th></tr></thead><tbody><tr><td>

User entry complete and checked

</td><td>

`userentry( int`\)

</td></tr><tr><td>

Return and input to `seekFactorA()`

</td><td>

`factorreturnA( int, int )`

</td></tr><tr><td>

Return and input to `seekFactorB()`

</td><td>

`factorreturnB( int, int )`

</td></tr><tr><td>

Immediately prior to the program exiting

</td><td>

`final()`

</td></tr><tbody></table>
### Exercise: Creating a dprime.d File

To reflect the previously described probe points and data, create a file named `dprime.d` and store the file in the same working directory as the other source files.

**Note:**

Typically, you would provide additional information in the `.d` file, such as stability attributes. For the sake of brevity, expedience, and simplicity, those details are not included in this introductory example.

\(Estimated completion time: less than 5 minutes\)

### Solution to Exercise: Creating a dprime.d File 

```
provider primeget
{
  probe query__userentry( int );
  probe query__maxcheckval( int, int );
  probe query__factorreturnA( int, int );
  probe query__factorreturnB( int, int );
  probe query__final();
};
```

### Example: Creating a .h File From a dprime.d File

The next step is to create a `.h` file from the `dprime.d` file, as shown here:

```
# `dtrace -h -s dprime.d`
```

The `dprime.h` file that is created contains a reference to each of the probe points that are defined in the `dprime.d` file.

Next, in the application source file, `primain.c`, we add a reference to the `#include "dprime.h"` file and add the appropriate probe macros at the proper locations.

In the resulting `primain.c` file, the probe macros \(shown in bold font for example purposes only\) are easy to recognize, as they appear in uppercase letters:

```
#include <stdio.h>
#include "primelib.h"
**\#include "dprime.h"**

/*
 * Nominal C program churning to provide a code base we might want to
 * instrument with D
*/

// Search for a divisor -- thereby proving composite value of the input.
int main()  {
  int targVal, divisor, factorA=0, factorB=0;

  printf( "Enter a positive target integer to test for prime status: " );
  scanf( "%d", &targVal );

  // Check that the user input is valid
  if( targVal < 2 ) {
    printf( "Invalid input value -- exiting now\n" );
    return -2;
  }
  **if \(PRIMEGET\_QUERY\_USERENTRY\_ENABLED\(\)\)
    PRIMEGET\_QUERY\_USERENTRY\(targVal\);**

  // Search for a divisor using method and function A
  int lastCheck;
  lastCheck = findMaxCheck( targVal );
  printf( "%d highest value to check as divisor\n", lastCheck );
  **if \(PRIMEGET\_QUERY\_MAXCHECKVAL\_ENABLED\(\)\)
    PRIMEGET\_QUERY\_MAXCHECKVAL\(lastCheck, targVal\);**

  factorA = seekFactorA( targVal, lastCheck );
  **if \(PRIMEGET\_QUERY\_FACTORRETURNA\_ENABLED\(\)\)
    PRIMEGET\_QUERY\_FACTORRETURNA\(factorA, targVal\);**

  // Search for a divisor using method and function B
  factorB = seekFactorB( targVal );
 **if \(PRIMEGET\_QUERY\_FACTORRETURNB\_ENABLED\(\)\)
    PRIMEGET\_QUERY\_FACTORRETURNB\(factorB, targVal\);**

  // Warn if the methods give different results
  if (factorA != factorB)
    printf( "%d does not equal %d! How can this be?\n", factorA, factorB );

  // Print results
  if( !factorA )
    printf( "%d is a prime number\n", targVal );
  else
    printf( "%d is not prime because there is a factor %d\n",
	    targVal, factorA );
  **if \(PRIMEGET\_QUERY\_FINAL\_ENABLED\(\)\)
    PRIMEGET\_QUERY\_FINAL\(\);**

  return 0;
}
```

**Note:**

Any *\** `_ENABLED()` probe will translate into a truth value if the associated probe is enabled \(some consumer is using it\), and a false value if the associated probe is not enabled.

Before continuing, ensure that the probes are enabled and appear as the macros listed in the `dprime.h` file. 

**Note:**

Make sure to include any desired values in the macros, if they exist, so that the probe can also identify those values.

Next, you will need to modify the `makefile` file. 

### Exercise: Directing makefile to Re-Create the dprime.h File

Add a target that instructs `dtrace` to re-create the `dprime.h` file in the event that changes are subsequently made to the `dprime.d` file. This step ensures that you do not have to manually run the `dtrace -h -s dprime.d` command if any changes are made.

This exercise also has you direct `dtrace` to create a `prime.o` file.

\(Estimated completion time: 10 minutes\)

### Solution to Exercise: Directing makefile to Re-Create the dprime.h File 

```
default: prime

# re-create new dprime.h if dprime.d file has been changed
dprime.h: dprime.d
	dtrace -h -s dprime.d

# compile the library primelib first
primelib.o: primelib.c
	gcc -c primelib.c

# compile the main program
primain.o: primain.c dprime.h
	gcc -c primain.c

# have dtrace post-process the object files
prime.o: dprime.d primelib.o primain.o
	dtrace -G -s dprime.d primelib.o primain.o -o prime.o

# link and create executable file "prime"
prime: prime.o
	gcc -Wl,--export-dynamic,--strip-all -o prime prime.o primelib.o primain.o dprime.h -lm


clean:
	-rm -f *.o
	-rm -f prime
	-rm -f dprime.h
```

### Example: Testing the Program

After creating a fresh build, test that the executable is still working as expected:

```
# `make clean`
rm -f *.o
rm -f prime
rm -f dprime.h


# `make`
gcc -c primelib.c
dtrace -h -s dprime.d
gcc -c primain.c
dtrace -G -s dprime.d primelib.o primain.o -o prime.o
gcc -Wl,--export-dynamic,--strip-all -o prime prime.o primelib.o primain.o dprime.h -lm
```

```
# `./prime`
Enter a positive target integer to test for prime status: 6799
Square root of 6799 is 82.456047
82 highest value to check as divisor
6799 is not prime because there is a factor 13
```

## Using USDT Probes

This section provides some practice in the nominal use of the USDT probes that were created in [Adding USDT Probes to an Application](dtrace-tutorial-TracingUserSpaceApplications.md#).

Initially, the probes are not visible because the application is not running with the probes, as shown in the following output:

```
# `dtrace -l -P 'prime*'`
  ID   PROVIDER            MODULE                          FUNCTION NAME
dtrace: failed to match prime*:::: No probe matches description
```

Start the application, but do not enter any value until you have listed the probes:

```
# `./prime `
Enter a positive target integer to test for prime status:
```

From another command line, issue a probe listing:

```
# `dtrace -l -P 'prime*'`
   ID      PROVIDER            MODULE                          FUNCTION NAME
 2475 primeget26556             prime                              main query-factorreturnA
 2476 primeget26556             prime                              main query-factorreturnB
 2477 primeget26556             prime                              main query-final
 2478 primeget26556             prime                              main query-maxcheckval
 2479 primeget26556             prime                              main query-userentry
```

**Note:**

The provider name is a combination of the defined `provider primeget`, from the `dprime.d` file, and the PID of the running application `prime`. The output of the following command displays the PID of prime:

```
# `ps aux | grep prime`
root 26556 0.0 0.0 7404 1692 pts/0 S+ 21:50 0:00 ./prime
```

If you want to be able to run USDT scripts for users other than `root`, the helper device must have the proper permissions. Alternatively, you can run the program with the probes in it as the `root` user. See [Example: Changing the Mode of the DTrace Helper Device](dtrace-tutorial-TracingUserSpaceApplications.md#) for more information about changing the mode of the DTrace helper device.

One method for getting these permissions is to run the following command to change the configuration so that users other than the `root` user can send probe provider information to DTrace:

```
# `setfacl -m u:guest:rw /dev/dtrace/helper`
```

Start the application again, but do not enter any values until the probes are listed:

```
# `./prime` 
Enter a positive target integer to test for prime status: 
```

From another command line, issue a probe listing:

```
# `dtrace -l -P 'prime*'`
   ID     PROVIDER            MODULE                    FUNCTION NAME
 2456 primeget2069             prime                        main query-factorreturnA
 2457 primeget2069             prime                        main query-factorreturnB
 2458 primeget2069             prime                        main query-final
 2459 primeget2069             prime                        main query-maxcheckval
 2460 primeget2069             prime                        main query-userentry
```

### Example: Using simpleTimeProbe.d to Show the Elapsed Time Between Two Probes

The following example shows how you would create a simple script that measures the time elapsed between the first probe and the second probe \(`query-userentry` to `query-maxcheckval`\).

```
/* simpleTimeProbe.d */

/* Show how much time elapses between two probes */

primeget*:::query-userentry
{
  self->t = timestamp; /* Initialize a thread-local variable with the time */
}

primeget*:::query-maxcheckval
/self->t != 0/
{
  timeNow = timestamp;
  /* Divide by 1000 for microseconds */
  printf("%s (pid=%d) spent %d microseconds between userentry & maxcheckval\n",
         execname, pid, ((timeNow - self->t)/1000));

  self->t = 0; /* Reset the variable */
}
```

Start the execution of the target application:

```
# `./prime`
Enter a positive target integer to test for prime status:
```

Then, run the DTrace script from another window:

```
# `dtrace -q -s simpleTimeProbe.d`
```

As the application is running, the output of the script is also running in parallel:

```
# `./prime `
Enter a positive target integer to test for prime status: 7921
Square root of 7921 is 89.000000
89 highest value to check as divisor
7921 is not prime because there is a factor 89
# `./prime `
Enter a positive target integer to test for prime status: 995099
Square root of 995099 is 997.546509
997 highest value to check as divisor
995099 is not prime because there is a factor 7
# `./prime `
Enter a positive target integer to test for prime status: 7921
Square root of 7921 is 89.000000
89 highest value to check as divisor
7921 is not prime because there is a factor 89
```

On the command line where the script is being run, you should see output similar to the following:

```
# `dtrace -q -s simpleTimeProbe.d`
prime (pid=2328) spent 45 microseconds between userentry & maxcheckval
prime (pid=2330) spent 41 microseconds between userentry & maxcheckval
prime (pid=2331) spent 89 microseconds between userentry & maxcheckval
`^C
`
```

### Example: Using timeTweenprobes.d to Show the Elapsed Time Between Each Probe

You can broaden the script to monitor all of the following probes in the application:

-   `query-userentry`

-   `query-maxcheckval`

-   `query-factorreturnA`

-   `query-factorreturnB`

-   `query-final`


```
/* timeTweenProbes.d */

/* show how much time elapses between each probe */

BEGIN
{
  iterationCount = 0;
}


primeget*:::query-userentry
{
  printf("%s (pid=%d) running\n", execname, pid);
  self->t = timestamp; /* Initialize a thread-local variable with time */
}

primeget*:::query-maxcheckval
/self->t != 0/
{
  timeNow = timestamp;
  printf(" maxcheckval spent %d microseconds since userentry\n",
         ((timeNow - self->t)/1000));  /* Divide by 1000 for microseconds */
  self->t = timeNow; /* set the time to recent sample */
}

primeget*:::query-factorreturnA
/self->t != 0/
{
  timeNow = timestamp;
  printf(" factorreturnA spent %d microseconds since maxcheckval\n",
         ((timeNow - self->t)/1000));  /* Divide by 1000 for microseconds */
  self->t = timeNow; /* set the time to recent sample */
}

primeget*:::query-factorreturnB
/self->t != 0/
{
  timeNow = timestamp;
  printf(" factorreturnB spent %d microseconds since factorreturnA\n",
         ((timeNow - self->t)/1000));  /* Divide by 1000 for microseconds */
  self->t = timeNow; /* set the time to recent sample */
}

primeget*:::query-final
/self->t != 0/
{
  printf(" prime spent %d microseconds from factorreturnB until ending\n",
         ((timestamp - self->t)/1000));
  self->t = 0; /* Reset the variable */
  iterationCount++;
}

END
{
  trace(iterationCount);
}
```

Again, start the execution of the target application first, then run the script from another window:

```
# `./prime `
Enter a positive target integer to test for prime status: 995099
Square root of 995099 is 997.546509
997 highest value to check as divisor
995099 is not prime because there is a factor 7
# `./prime `
Enter a positive target integer to test for prime status: 7921
Square root of 7921 is 89.000000
89 highest value to check as divisor
7921 is not prime because there is a factor 89
# `./prime `
Enter a positive target integer to test for prime status: 95099
Square root of 95099 is 308.381256
308 highest value to check as divisor
95099 is not prime because there is a factor 61
# `./prime `
Enter a positive target integer to test for prime status: 95099
Square root of 95099 is 308.381256
308 highest value to check as divisor
95099 is not prime because there is a factor 61
# `./prime `
Enter a positive target integer to test for prime status: 5099         
Square root of 5099 is 71.407280
71 highest value to check as divisor
5099 is a prime number
```

The corresponding output from the script is similar to the following:

```
# `dtrace -q -s ./timeTweenProbes.d`
prime (pid=2437) running
 maxcheckval spent 96 microseconds since userentry
 factorreturnA spent 9 microseconds since maxcheckval
 factorreturnB spent 6 microseconds since factorreturnA
 prime spent 9 microseconds from factorreturnB until ending
prime (pid=2439) running
 maxcheckval spent 45 microseconds since userentry
 factorreturnA spent 10 microseconds since maxcheckval
 factorreturnB spent 7 microseconds since factorreturnA
 prime spent 9 microseconds from factorreturnB until ending
prime (pid=2440) running
 maxcheckval spent 43 microseconds since userentry
 factorreturnA spent 11 microseconds since maxcheckval
 factorreturnB spent 8 microseconds since factorreturnA
 prime spent 10 microseconds from factorreturnB until ending
prime (pid=2441) running
 maxcheckval spent 53 microseconds since userentry
 factorreturnA spent 10 microseconds since maxcheckval
 factorreturnB spent 7 microseconds since factorreturnA
 prime spent 10 microseconds from factorreturnB until ending
prime (pid=2442) running
 maxcheckval spent 40 microseconds since userentry
 factorreturnA spent 9 microseconds since maxcheckval
 factorreturnB spent 48 microseconds since factorreturnA
 prime spent 10 microseconds from factorreturnB until ending
`
^C`
5
```

As is observed in the previous example, there is now a set of DTrace features that can be used with the probes that were created.

