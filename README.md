GGGGC is a precise, generational, moving garbage collector for C. It is not as
general-purpose as a conservative collector (e.g. libgc), but is a good
starting place for implementing new virtual machines.

GGGGC is hugely portable. The generational collector requires at least a 32-bit
system, but the portable mark and sweep collector works on 16-bit systems, and
even 8-bit systems with 16-bit addressing. Fairly small adjustments are
necessary to work in extremely constrained environments.

GGGGC is also intended to be a garbage collector for learning. The details of
collection itself are separated from the murkier details of OS-level memory
management and C/C++ issues, making it a clean environment for studying how
garbage collection works without being bogged down by language or platform
details.

To use GGGGC, simply include `ggggc/gc.h` and observe the restrictions it
enforces on your code. Excluding the threading library, all public API macros
begin with `GGC_`, and there are no public API functions.

GGGGC is not intended to be installed as a system library. Every program which
needs GGGGC should include it.


Learning
========

GGGGC has been used as a baseline for teaching garbage collection. Simply
remove the collector implementations, `ggggc/gc-*.h` and `collector-*.h`, and
optionally replace them with stubs, to create an effective baseline. Depending
on context, it may also be worthwhile to remove the threading code.

Issues such as OS-specific pool allocation, creating and managing type
descriptors, thread concerns etc. are handled separately, so students can focus
on garbage collection, rather than its various tertiary concerns.


Types
=====

GGGGC types need to be defined in such a way that the collector knows where the
pointers are. This is done with the `GGC_TYPE`, `GGC_MDATA`, `GGC_MPTR`,
`GGC_END_TYPE` and `GGC_PTR` like so:

    GGC_TYPE(ListOfFoosAndInts)
        GGC_MPTR(ListOfFoosAndInts, next);
        GGC_MPTR(Foo, fooMember);
        GGC_MDATA(int, intMember);
    GGC_END_TYPE(ListOfFoosAndInts,
        GGC_PTR(ListOfFoosAndInts, next)
        GGC_PTR(ListOfFoosAndInts, fooMember)
        )

This is more verbose than a conventional type declaration, of course, but gives
the GC the information it needs. New objects are created with `GGC_NEW`.

Users may also create their own descriptors, so long as they follow the correct
format, in which case the object described merely needs to start with its
descriptor pointer. Look at struct `GGGGC_Descriptor` for the format.

Because generational garbage collection requires a write barrier, members of
GC'd types must be accessed through the `GGC_R*` and `GGC_W*` family of macros.
Reading and writing pointer members is done through `GGC_RP` and `GGC_WP`,
respectively. Reading and writing data (non-pointer or non-GC'd) members is
done through `GGC_RD` and `GGC_WD`. For instance:

    newObj = GGC_NEW(ListOfFoosAndInts);
    GGC_WD(newObj, intMember, 42);
    GGC_WP(list, next, newObj);
    if (GGC_RP(list, fooMember) != NULL)
        GGC_WP(list, fooMember, NULL);

Each GGGGC type also has an array type, simply named the same as the user type
with `Array` at the end, e.g. `ListOfFoosAndIntsArray`. Arrays of GC'd pointers
are created with `GGC_NEW_PA`. Arrays have a `length` member, which should not
be changed, and their array content may be accessed with `GGC_RAP`
(read-array-pointer) and `GGC_WAP` (write-array-pointer). Example:

    arrayOfLists = GGC_NEW_PA(ListOfFoosAndInts, 2);
    GGC_WAP(arrayOfLists, 0, newObj);
    GGC_WAP(arrayOfLists, 1, NULL);
    for (i = 0; i < arrayOfLists->length; i++)
        printList(GGC_RAP(arrayOfLists, i));

Arrays of non-GGGGC (i.e., non-pointer or non-GC'd) types are also supported.
They are named `GGC_*_Array`, where `*` is the member type. Many common data
types have array types provided by `ggggc/gc.h`: `char`, `short`, `int`,
`unsigned`, `long`, `float`, `double` and `size_t`. To declare the type for an
array of some other data type, the macro `GGC_DA_TYPE` is provided, e.g.:

    GGC_DA_TYPE(MyStructuralType)

Arrays of non-GGGGC types are created and accessed similarly to arrays of GC'd
types, but with `GGC_NEW_DA`, `GGC_RAD` and `GGC_WAD` in place of `GGC_NEW_PA`,
`GGC_RAP` and `GGC_WAP`. For instance:

    GGC_int_Array ints = NULL;
    ...
    ints = GGC_NEW_DA(int, 42);
    for (i = 0; i < ints->length; i++)
        GGC_WAD(ints, i, i);
    for (i = ints->length - 1; i >= 0; i--)
        printf("%d\n", GGC_RAD(ints, i));


Functions
=========

Because GGGGC is precise, it is necessary to inform the garbage collector of
every pointer in the stack. This is done with the `GGC_PUSH_*` macros, where
`*` is the number of pointers being described. You must assure that every
pointer is valid or NULL before pushing it, and furthermore must not call the
collector (`GGC_NEW`) before pushing pointers. For example:

    int addList(ListOfFoosAndInts list) {
        ListOfFoosAndInts newObj = NULL;
        int sum = 0;
        GGC_PUSH_2(list, newObj);
        if (GGC_R(list, next))
            sum += addList(GGC_R(list, next));
        return sum;
    }

Pushing pointers is *vital* to the correctness of GGGGC. If you fail to push
your pointers correctly, your program will seg fault, behave strangely, or
otherwise fail. Pushed pointers are automatically popped when compiling with GCC
or C++, and nasty hacks are used to make it *usually* pop even when compiling
with neither, but if you're not using GCC (or a compatible compiler like clang),
you're highly advised to use C++ to make this reliable.

If you'd like to control the popping process yourself, you can use
`GGC_PUSH_MANUAL_*` and `GGC_POP_MANUAL`. Be careful to use `GGC_POP_MANUAL`
with `GGC_PUSH_MANUAL_*`, as `GGC_POP` is a macro to ensure popping from a
normal `GGC_PUSH_*` in a non-GCC, non-C++ system.

You must be careful in C to not store pointers to GC'd objects in temporary
locations through function calls; in particular, do not call functions within
the arguments of other functions, as those function calls may yield and destroy
your pointers.


GGGGC from C++
==============

Many of the most annoying aspects of using a precise GC can be automated in C++,
and if `ggggc/gc.h` is included in C++ mode, all of these automations are
available.

In C++, write (and read) barriers are implemented through operator overloading,
so fields may be accessed with, e.g., `list->fooMember`.

To automate pushing and popping values to/from the stack, in C++, the `GGC<>`,
`GGCAP<>`, and `GGCAD<>` types are provided. `GGC` is a class template, usable
only in the stack (i.e., arguments and variables of functions) that contains a
pointer and automatically pushes it to and pops it from the pointer stack, while
additionally providing access to all the fields of the underlying object with
`->`. For instance, the `ListOfFoosAndInts` code above can be rewritten in C++
as follows:

```
int addList(GGC<ListOfFoosAndInts> list) {
    GGC<ListOfFoosAndInts> newObj;
    int sum = 0;
    if (list->next)
        sum += addList(list->next);
}
```

`GGCAP<>` and `GGCAD<>` are subclasses of `GGC<>` that provide convenience
methods for using arrays. In each case, both the element type and the array type
are parameters: for instance, `GGCAP<Foo, FooArray>` or
`GGCAD<int, GGC_int_Array>`. Elements can be read with `[]` (like a typical
array), but to write, you must use `array.put(index, value)`.


Configuration
=============

Some of GGGGC's behavior is configurable through preprocessor definitions. The
following definitions are available:

 * `GGGGC_COLLECTOR`: Which collector to use. Presently, two collectors are
   available. The "gembc" collector (generational, en-masse promotion,
   break-table compaction) is default on most systems. The "portablems"
   (portable mark-and-sweep) collector is used by default on 16-bit systems.
   Setting this to other values will cause the inclusion of
   `gc-GGGGC_COLLECTOR.h` and `collector-GGGGC_COLLECTOR.c`, which together may
   implement alternative collection schemes.

 * `GGGGC_GENERATIONS`: Sets the number of generations used by the gembc
   collector. `GGGGC_GENERATIONS=1` will yield a non-generational collector.
   `GGGGC_GENERATIONS=2` will yield a conventional generational collector with a
   nursery and long-lived pool, and is the default. Higher values yield more
   generations.

 * `GGGGC_POOL_SIZE`: Sets the size of allocation pools, as a power of two.
   Default is 24 (16MB), except on 16-bit systems, where it's 12 (4KB).

 * `GGGGC_CARD_SIZE`: Sets the size of remembered set cards in the gembc
   collector, as a power of two. Default is 12 (4KB).

 * `GGGGC_DEBUG`: Enables all debugging options.

 * `GGGGC_DEBUG_MEMORY_CORRUPTION`: Enables debugging checks for memory
   corruption.

 * `GGGGC_DEBUG_REPORT_COLLECTIONS`: Enables reporting of collection
   statistics.

 * `GGGGC_DEBUG_TINY_HEAP`: Restrict the heap size to the smallest feasible
   size. Note that if the program strictly requires more space, it will fail.
   This is essentially a stress test.

 * `GGGGC_NO_GNUC_FEATURES`: Disables all GNU C features when `__GNUC__` is
   defined. (GNU C features are, naturally, disabled on non-GNU-compatible
   compilers regardless)

 * `GGGGC_NO_GNUC_CLEANUP`: Disable use of `__attribute__((cleanup(x)))`

 * `GGGGC_NO_GNUC_CONSTRUCTOR`: Disable use of `__attribute__((constructor))`

 * `GGGGC_NO_THREADS`: Disables all threading code. This will be set by default
   if no thread-local storage or no threading library can be found, but may be
   set explicitly to avoid the preprocessor warning in these cases.

 * `GGGGC_USE_SBRK`: Use `sbrk` instead of a smarter (or at least more
   system-specific) allocator.

 * `GGGGC_USE_MALLOC`: Use `malloc` instead of a smarter allocator. This is
   necessary on systems that don't have any smarter allocator (even `sbrk`!),
   as `sbrk` is used as the fallback.

 * `GGGGC_FEATURE_FINALIZERS`: Enable the finalizers feature. Finalizers are
   documented in [doc/FINALIZERS.md](FINALIZERS.md).

 * `GGGGC_FEATURE_TAGGING`: Enables the internal tagging feature. Tagging is
   documented in [doc/TAGGING.md](TAGGING.md).

 * `GGGGC_FEATURE_EXTTAG`: Enables the external tagging feature. Tagging is
   documented in [doc/TAGGING.md](TAGGING.md).

 * `GGGGC_FEATURE_JITPSTACK`: Enables the JIT pointer stack feature. JIT pointer
   stacks are documented in [doc/JITPSTACK.md](JITPSTACK.md).


Portability
===========

GGGGC should work almost anywhere. It's tested on conforming POSIX systems,
Windows, and DOS. GGGGC configures itself to the target system by feature
macros, such as `_POSIX_VERSION` for POSIX and `_WIN32` for windows, and thus
requires no `configure` script or similar.

Most components of GGGGC are intrinsically portable and should not require
modification to support a new platform. The two components that are unportable
are the OS-level allocator (see the beginning of `allocate.c`, which includes
e.g. `allocate-mmap.c`) and thread support. If no thread support is needed, it
can be excluded with the configuration macro `GGGGC_NO_THREADS`. Otherwise, new
thread intrinsics can be added in `ggggc/threads.h` and `threads.c`. For
OS-level allocation, if nothing else suffices, `malloc` can be used with a
flag, but this is extremely wasteful.


Optional Features
=================

Certain features are important for some users but unimportant for others. These
features are implemented through feature macros starting with `GGGGC_FEATURE_`.
See the documentation of those macros above for more information.


Your Mileage May Vary
=====================

Beyond the above, there is no further documentation. Please read ggggc/gc.h
itself to get further clues.
