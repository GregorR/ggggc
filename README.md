GGGGC is a precise, generational, copying garbage collector for C. It is not as
general-purpose as a conservative collector (e.g. libgc), but is a good
starting place for implementing new virtual machines.

To use GGGGC, simply include `ggggc/gc.h` and observe the restrictions it
enforces on your code. Excluding the threading library, all public API macros
begin with `GGC_`, and there are no public API functions.

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

Because generational garbage collection requires a write barrier, pointer
members of types must be accessed through the `GGC_R` and `GGC_W` macros:

    newObj = GGC_NEW(ListOfFoosAndInts);
    GGC_W(list, next, newObj);
    if (GGC_R(list, fooMember) != NULL)
        GGC_W(list, fooMember, NULL);

Because GGGGC is precise, it is necessary to inform the garbage collector of
every pointer in the stack. This is done with the `GGC_PUSH_*` macros, where
`*` is the number of pointers being described. You must assure that every
pointer is valid or NULL before pushing it, but furthermore must not call the
collector (GGC_NEW) before pushing pointers. For example:

    int addList(ListOfFoosAndInts list) {
        ListOfFoosAndInts newObj = NULL;
        int sum = 0;
        GGC_PUSH_2(list, newObj);
        if (GGC_R(list, next))
            sum += addList(GGC_R(list, next));
        return sum;
    }

You must be careful in C to not store pointers to GC'd objects in temporary
locations through function calls; in particular, do not call functions within
the arguments of other functions, as those function calls may yield and destroy
your pointers.

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

Beyond the above, there is no further documentation. Please read ggggc/gc.h
itself to get further clues.
