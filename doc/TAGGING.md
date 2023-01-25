By default, GGGGC is precise, and assumes that the compiled code always knows
when a value is a pointer or not. Many dynamically typed languages need to store
values which may or may not be GC'd pointers. GGGGC supports tagging of values
to distinguish between GC'd and non-GC'd values.

GGGGC supports two modes of tagging: internal and external. In internal tagging,
potential pointers themselves are tagged. In external tagging, every potential
pointer has an associated (but separately stored) tag. Internal tagging
restricts how much data the tag may store, as it uses the alignment bits of the
pointer. External tagging gives every tag 8 bits.

Internal and external tagging are incompatible. Do not attempt to enable both.


Internal tagging
================

Use the macro `GGGGC_FEATURE_TAGGING` to enable internal tagging. When internal
tagging is enabled, only aligned pointers will be followed. Thus, you may store
non-pointer values as unaligned pointers.

For instance, 63-bit integers (on a 64-bit system) can be stored in pointers by
storing a value shifted left by one, with the last bit set to 1.

On a 32-bit system, the last two bits are used for alignment, so you may
potentially have as many as four unique tags. On 64-bit systems, the last three
bits are used for alignment, so you may have as many as eight tags. On 16-bit
systems, only the last bit is used for alignment, so it's only possible to
distinguish GC'd and non-GC'd data.


External tagging
================

Use the macro `GGGGC_FEATURE_EXTTAG` to enable external tagging. When external
tagging is enabled, each potential pointer is associated with a byte that
describes its type. Usually, GGGGC itself only cares whether the tag byte is
even or odd: even tags indicated GC'd values, and odd tags indicate non-GC'd
values. However, the values 255, 254, 253, and 252 are reserved for internal
use. Thus, the user has 126 tags for GC'd values and 126 tags for non-GC'd
values.

Tags are stored in object descriptors, so objects with different tags must have
distinct descriptors, even if they're otherwise identical. This means that code
using GGGGC must be prepared to create new descriptors on the fly.

External tagging interacts with the [JITPSTACK.md](JIT pointer stack); its
interaction is documented in the JIT pointer stack's documentation file.
