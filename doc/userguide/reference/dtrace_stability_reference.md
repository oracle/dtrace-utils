
# DTrace Stability Reference

DTrace provides a mechanism to track the stability of interfaces and their architecture dependencies. This reference provides detail on how attributes are stored and described and their values.

## DTrace Interface Stability Attributes

DTrace describes interfaces by using a triplet of attributes consisting of two stability levels and one dependency class. By convention, the interface attributes are written in the following order and are separated by slashes:

```
*name\_stability* / *data\_stability* / *dependency\_class*
```

The *name stability* of an interface describes the stability level that's associated with its name, as it appears in a D program or on the `dtrace` command line. For example, the `execname` D variable is a Stable name.

The *data stability* of an interface is distinct from the stability that's associated with the interface name. This stability level describes the commitment to maintain the data formats that are used by the interface and any associated data semantics.

The *dependency class* of an interface is distinct from its name and data stability and describes whether the interface is specific to the current operating platform or microprocessor.

DTrace and the D compiler track the stability attributes for all the following DTrace interface entities: providers, probe descriptions, D variables, D functions, types, and program statements.

Stability attributes are computed by selecting the minimum stability level and class from the corresponding values for each interface attributes triplet.

The DTrace utility can report on the calculated stability of a D program when run with the `-v` option. Use the -e option to prevent DTrace from running the program and to restrict output to only provide the report. For example, you can run:

```
sudo dtrace -ev -s *myscript.d*
```

Output similar to the following is displayed:

```
Stability attributes for description dtrace:::BEGIN:

	Minimum Probe Description Attributes
		Identifier Names: Stable
		Data Semantics:   Stable
		Dependency Class: Common

	Minimum Statement Attributes
		Identifier Names: Stable
		Data Semantics:   Private
		Dependency Class: Common

dtrace:::BEGIN

	Probe Description Attributes
		Identifier Names: Stable
		Data Semantics:   Stable
		Dependency Class: Common

	Argument Attributes
		Identifier Names: Stable
		Data Semantics:   Stable
		Dependency Class: Common

	Argument Types
		None

```

You can use the `-x amin=_attributes_` option with the `dtrace` command to force the D compiler to produce an error whenever any attributes computation results in a triplet of attributes less than the minimum values that you specify on the command line. Note that attributes are specified with three labels that are delimited `/`, according to the standard notation to describe stability. For example:

```
sudo dtrace -x amin=*Evolving/Evolving/Common* -s *myscript.d*
```

Stability attributes are computed for a probe description by taking the minimum stability attributes of all the specified probe description fields, according to the attributes that are published by the provider. DTrace providers export a stability attributes triplet for each of the four description fields for all the probes published by that provider. Therefore, a provider's name can have a greater stability than the individual probes that it exports. For simplicity, most providers use a single set of attributes for all the individual module function name values they publish. Providers also specify attributes for the `args[]` array because the stability of any probe arguments varies by provider.

If the provider field isn't specified in a probe description, then the description is assigned the Unstable/Unstable/Common stability attributes because the description might end up matching probes of providers that don't yet exist when used on a future Linux release. As such, DTrace doesn't provide guarantees about the future stability and behavior of the program. Always explicitly specify the provider when writing D program clauses. In addition, any probe description fields that contain pattern matching characters or macro variables, such as `$1`, are treated as unspecified because these description patterns might expand to match providers or probes to be released in future versions of DTrace and Linux.

## Stability Levels

Stability levels describe the stability of software entities and DTrace interfaces. DTrace stability levels indicate how likely D programs and layered tools are to require corresponding changes when you upgrade or change the software stack.

<table><thead><tr><th>

Stability Value

</th><th>

Description

</th></tr></thead><tbody><tr><td>

Internal

</td><td>

The interface is private to DTrace and represents an implementation detail of DTrace. Internal interfaces might change in minor or micro releases.

</td></tr><tr><td>

Private

</td><td>

The interface is private to Oracle and represents an interface developed for use by other Oracle products that aren't yet publicly documented for use by customers and ISVs \(independent software vendors\). Private interfaces might change in minor or micro releases.

</td></tr><tr><td>

Obsolete

</td><td>

The interface is available in the current release but is scheduled to be removed, most likely in a future minor release. The D compiler might produce warning messages if you try to use an Obsolete interface.

</td></tr><tr><td>

External

</td><td>

The interface is controlled by an entity other than Oracle. Oracle makes no claims regarding either source or binary compatibility for External interfaces between any two releases. Applications based on these interfaces might not work in future releases, including patches that contain External interfaces.

</td></tr><tr><td>

Unstable

</td><td>

The interface provides developers early access to new or changing technology or to an implementation artifact that's essential for observing or debugging system behavior for which a more stable solution is expected in the future. Oracle makes no claims about either source or binary compatibility for Unstable interfaces from one minor release to another.

</td></tr><tr><td>

Evolving

</td><td>

The interface might eventually become Standard or Stable but is still in transition. When non-upward, compatible changes become necessary, they occur in minor and major releases. These changes are avoided in micro releases whenever possible. If such a change is necessary, it's documented in the release notes for the affected release. Also, when feasible, migration aids are provided for binary compatibility and continued D program development.

</td></tr><tr><td>

Stable

</td><td>

The interface is a mature interface.

</td></tr><tr><td>

Standard

</td><td>

The interface complies with an industry standard. The corresponding documentation for the interface describes the standard to which the interface conforms. Standards are typically controlled by a standards development organization. Changes can be made to the interface in accordance with approved changes to the standard. This stability level can also apply to interfaces that have been adopted \(without a formal standard\) by an industry convention. Availability is provided for only the specified versions of a standard; availability in later versions isn't guaranteed.

</td></tr><tbody></table>

## Dependency Classes

Dependency classes are used to describe architectural dependencies for interfaces in DTrace.

<table><thead><tr><th>

Dependency Class

</th><th>

Description

</th></tr></thead><tbody><tr><td>

Unknown

</td><td>

The interface has an unknown set of architectural dependencies. DTrace doesn't necessarily know the architectural dependencies of all entities, such as the data types defined in the OS implementation. The Unknown label is typically applied to interfaces of very low stability for which dependencies can't be computed. The interface might not be available when using DTrace on *any* architecture other than what you're currently using.

</td></tr><tr><td>

CPU

</td><td>

The interface is specific to the CPU model of the current system. Interfaces with CPU model dependencies might not be available on other CPU implementations, even if those CPUs export the same instruction set architecture \(ISA\).

</td></tr><tr><td>

Platform

</td><td>

The interface is specific to the hardware platform for the current system. A platform typically associates a set of system components and architectural characteristics. To display the current platform name, use the `uname -i` command. The interface might not be available on other hardware platforms.

</td></tr><tr><td>

Group

</td><td>

The interface is specific to the hardware platform group for the current system. A platform group typically associates a set of platforms with related characteristics together under a single name. To display the current platform group name, use the `uname -m` command. The interface is available on other platforms in the platform group, but it might not be available on hardware platforms that aren't members of the group.

</td></tr><tr><td>

ISA

</td><td>

The interface is specific to the ISA that's available for the microprocessors on the current system. The ISA describes a specification for software that can be run on the microprocessor, including details such as assembly language instructions and registers.

</td></tr><tr><td>

Common

</td><td>

The interface is common to all Linux platforms, regardless of the underlying hardware. DTrace programs and layered applications that depend only on Common interfaces can be run and deployed on other Linux platforms with the same Linux and DTrace revisions. Most DTrace interfaces are Common, so you can use them wherever you use Linux.

</td></tr><tbody></table>
