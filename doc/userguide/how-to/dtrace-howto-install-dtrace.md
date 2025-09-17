# Install DTrace{#install_dtrace}

The following instructions provide steps to install DTrace on different Linux distributions and to verify that the installation was successful.

**Parent topic:**[Get Started With DTrace](../how-to/dtrace-guide.md)

## Build and Install DTrace on Gentoo Linux {#build_install_dtrace_gentoo_linux}

  ```
  emerge dev-debug/dtrace
  ```
  For more details related to Gentoo DTrace, please see the [DTrace Gentoo wiki](https://wiki.gentoo.org/wiki/DTrace)
  For DTrace package info, please see the [Gentoo package repository for DTrace](https://packages.gentoo.org/packages/dev-debug/dtrace)

## Install DTrace on Oracle Linux


### Install DTrace on Oracle Linux 10 {#install_dtrace_oracle_linux_10}

1.  Enable the yum repository.

    If running on an x86 platform, enable the `ol10_UEKR8` yum repository for the system.

    ```
    sudo dnf config-manager --enable ol10_UEKR8
    ```

    **Note:**

    Oracle releases UEK and DTrace packages in the `baseos` repository for aarch64 platforms. You don't need to enable any other repositories to access the DTrace packages for aarch64 platforms.

2.  Install DTrace.

    Install the `dtrace` package.

    ```
    sudo dnf install -y dtrace
    ```


### Install DTrace on Oracle Linux 9 {#install_dtrace_oracle_linux_9}

1.  Enable the yum repository.

    If running on an x86 platform, enable the `ol9_UEKR7` yum repository for the system.

    ```
    sudo dnf config-manager --enable ol9_UEKR7
    ```

    **Note:**

    Oracle releases UEK and DTrace packages in the `baseos` repository for aarch64 platforms. You don't need to enable any other repositories to access the DTrace packages for aarch64 platforms.

2.  Install DTrace.

    Install the `dtrace` package.

    ```
    sudo dnf install -y dtrace
    ```


### Install DTrace on Oracle Linux 8 {#install_dtrace_oracle_linux_8}

1.  Enable the yum repository.

    If running on an x86 platform, enable either the `ol8_UEKR6` or `ol8_UEKR7` yum repository for the system.

    For example, run:

    ```
    sudo dnf config-manager --enable ol8_UEKR7
    ```

    **Note:**

    Oracle releases UEK and DTrace packages in the `baseos` repository for aarch64 platforms. You don't need to enable any other repositories to access the DTrace packages for aarch64 platforms.

2.  Install DTrace.

    Install the `dtrace` package.

    ```
    sudo dnf install -y dtrace
    ```


### Install DTrace on Oracle Linux 7 {#install_dtrace_oracle_linux_7}

**Warning:**

Oracle Linux 7 is now in Extended Support. See [Oracle Linux Extended Support](https://www.oracle.com/a/ocom/docs/linux/oracle-linux-extended-support-ds.pdf) and [Oracle Open Source Support Policies](https://www.oracle.com/us/support/library/enterprise-linux-support-policies-069172.pdf) for more information.

Migrate applications and data to Oracle Linux 8, Oracle Linux 9, or Oracle Linux 10, as soon as possible.

1.  Enable the `ol7_UEKR6` yum repository.

    For example, if you have `yum-utils` installed, run:

    ```
    sudo yum-config-manager --enable ol7_UEKR6
    ```

2.  Install DTrace.

    Install the `dtrace` and `libdtrace-ctf` packages:

    ```
    sudo yum install -y dtrace libdtrace-ctf
    ```


## Verify the DTrace Installation {#verify_install}

Check that DTrace is installed to the correct location and verify the DTrace version.

1.  Confirm DTrace is installed into `/usr/sbin/dtrace`.

    ```
    ls -lah /usr/sbin/dtrace
    ```

2.  Display the DTrace version number.

    ```
    dtrace -V
    ```

    The output looks similar to:

    ```
    dtrace:  D 2.0.3
    ```


