GGGGC optionally supports finalizers, when using the macro
`GGGGC_FEATURE_FINALIZERS`. When using finalizers, try to make sparing use of
them, as they can be fairly expensive.

A finalizer for an object must be a function of the type `void (*)(void *)`. Its
only argument is the object being finalized. Because the object being finalized
is an argument to the finalizer, like in many GC systems with finalizers,
finalized objects actually survive the collection in which they're finalized,
and are collected in the next collection.

To set a finalizer on an object, use `GGC_FINALIZE(obj, finalizer)`. It is not
possible to remove finalizers from objects.
