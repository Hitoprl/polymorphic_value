# polymorphic_value
C++ polymorphic_value, a type erasure wrapper for polymorphism without references or pointers.

This is a type that allows passing objects that are a subclass of some base class that are copy-constructible. It implements SBO to avoid heap allocation on small enough types. The SBO buffer size is configurable as a template parameter, as well as the SBO alignment, and a switch to allow heap allocations.

For performance reasons, the type doesn't have an "empty" state, it always holds an object. To support not having a value, use `std::optional`.

This code is just an exercise for me. The units tests might not be exhaustive. Exception safety is not tested, so I wouldn't be suprised if it isn't exception safe. Also, I'm aware that there are other implementations out there, including one trying to get into the standard.

