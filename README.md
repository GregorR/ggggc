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

`GGC_POP()` is also necessary, but GGGGC redefines `return` to assure that it
is done automatically. This has two implications: (1) It is necessary to
`return` from every function in which you push, even if the return type is
`void`, and (2) if another library also redefines `return`, it is necessary to
pop manually in files which use both libraries. The behavior of overriding
`return` can be disabled by defining `GGC_NO_REDEFINE_RETURN`.

You must be careful in C to not store pointers to GC'd objects in temporary
locations through function calls; in particular, do not call functions within
the arguments of other functions, as those function calls may yield and destroy
your pointers.

Beyond the above, there is no further documentation. Please read ggggc/gc.h
itself to get further clues.
