GGGGC is a precise, generational, copying garbage collector for C. It is not as
general-purpose as a conservative collector (e.g. libgc), but is a good
starting place for implementing new virtual machines.

To use GGGGC, simply include `ggggc/gc.h` and observe the restrictions it
enforces on your code. Excluding the threading library, all public API macros
begin with `GGC_`, and there are no public API functions.

GGGGC is not at present intended to be installed as a system library. Every
program which needs GGGGC should include it.


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
collector (GGC_NEW) before pushing pointers. For example:

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
otherwise fail.

You must be careful in C to not store pointers to GC'd objects in temporary
locations through function calls; in particular, do not call functions within
the arguments of other functions, as those function calls may yield and destroy
your pointers.


Configuration
=============

Some of GGGGC's behavior is configurable through preprocessor definitions. The
following definitions are available:

 * `GGGGC_GENERATIONS`: Sets the number of generations. `GGGGC_GENERATIONS=1`
   will yield a non-generational collector. `GGGGC_GENERATIONS=2` will yield a
   conventional generational collector with a nursery and long-lived pool, and
   is the default. Higher values yield more generations, which is generally
   pointless.

 * `GGGGC_POOL_SIZE`: Sets the size of allocation pools, as a power of two.
   Default is 24 (16MB).

 * `GGGGC_CARD_SIZE`: Sets the size of remembered set cards, as a power of two.
   Default is 12 (4KB).

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

 * `GGGGC_USE_MALLOC`: Use `malloc` instead of a smarter allocator. `malloc`
   will be used by default if no smarter allocator can be found, but this may
   be set explicitly to avoid the preprocessor warning in this case.


Portability
===========

GGGGC should work on any conforming POSIX system. It also works on Windows, in
true Windows mode (e.g. Microsoft Visual Studio or MingW) or POSIX mode with
Cygwin. 32- and 64-bit systems are supported. GGGGC configures itself to the
target system by feature macros, such as `_POSIX_VERSION` for POSIX and
`_WIN32` for windows, and thus requires no `configure` script or similar.

Most components of GGGGC are intrinsically portable and should not require
modification to support a new platform. The two components that are unportable
are the OS-level allocator (see the beginning of `allocate.c`, which includes
e.g. `allocate-mmap.c`) and thread support. If no thread support is needed, it
can be excluded with the configuration macro GGGGC_NO_THREADS. Otherwise, new
thread intrinsics can be added in `ggggc/threads.h` and `threads.c`. For
OS-level allocation, Iif nothing else suffices, `malloc` can be used, but this
is extremely wasteful.

Support for true 16-bit systems is likely infeasible. 16-bit systems compiling
in "huge mode" (i.e., with 32-bit data pointers) should be supported with
little effort beyond reducing the default pool size.


Patching
========

Certain features are important for some users but unimportant for others. To
avoid slowing down the mainline implementation, these features are available as
patches in the `patches` directory. You may use them by either including a
pre-patched version or, if you use Mercurial or Git subrepositories to include
GGGGC and thus cannot include patches directly, may include GGGGC in an unused
subdirectory, then use `make patch` to copy a patched version into ../ggggc.
`make patch` takes one argument, `PATCHES`, as in `make patch
PATCHES=jitpstack` or `make patch PATCHES="jitpstack tagging"`. `make patch` is
careful not to redo work, and so is safe to run as part of a larger build
process.


Your Mileage May Vary
=====================

Beyond the above, there is no further documentation. Please read ggggc/gc.h
itself to get further clues.
