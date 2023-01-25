The pointer stacks implemented in GGGGC by default (with `GGGGC_PUSH_*`) are
intended for interaction between C or C++ code and the GC. They are not
particularly useful for generated code (e.g. JITs) to interact with GGGGC.

Use the macro `GGGGC_FEATURE_JITPSTACK` to enable JIT pointer stacks. Allocate
as much space as you'd like for each thread that will run JIT code, and set the
thread-local `ggc_jitPointerStack` and `ggc_jitPointerStackTop` to the *high
end* of that space. Then, when executing code, decrease `ggc_jitPointerStack` to
request more stack space, and increase it to release stack space. Generally
speaking, you should make the JIT pointer stack a distinct register, and
decrement/increment it like the conventional stack pointer, then copy that
register into `ggc_jitPointerStack` when you yield to the GC.

The JIT pointer stack should have *exclusively* GC pointers on it. Be careful to
blank (or set correctly) all space allocated.


Interaction with tagging
========================

The JIT pointer stack is compatible with either internal or external tagging
(or, of course, neither). With internal tagging, nothing special is required;
values on the JIT pointer stack will be taken to be pointers if they're aligned.

With external tagging, the JIT pointer stack must have a different form. When
you allocate space on the JIT pointer stack, you must also allocate space for
tags.

The arrangement is that a word is used for tags as often as it is needed. For
instance, on a 64-bit system, every ninth word is used for tags: each tag is one
byte (8 bits), so a tag word holds the tags for the eight following words. The
low-order byte of the tagging word is the tag for the immediately next word, the
second byte of the tagging word is for the word after that, etc. On a 32-bit
system, the tagging word contains the tags for the next four words, and on a
16-bit system, it contains the tags for the next two words.

The special tag `0xFF` indicates the end of a tagging word. This can be used to
allocate less than a tagging word worth of words. For instance, if you only need
two words, you can allocate three words, of which the first word will serve as
the tagging word, and set the third tag to `0xFF`.
