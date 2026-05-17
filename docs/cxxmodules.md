# C++ Modules

The Draconic Engine native layer is written in C++ and its native libraries are structured using modules.
Modules allow grouping of C++ code in a standard encapsulated way.

Modules provide a new compilation mechanism where the compiler maps groups of symbols to names.
Existing legacy C++ code remains compatible, but as an implementation detail. Modules are
therefore best used when wrapping legacy source/header code.

There are three types of translation units when one includes modules:
- Regular translation unit (the source/header kind)
- Module interface unit
- Module implementation unit

Module interfaces export the public declaration for the module. They play a role which is the equivalent to that of
headers in the traditional C++ translation unit.

Module implementation units do not export publicly, although all the code in a module implementation is visible
to the linker, thus allowing to split the interface declaration from the implementation.

> Declare module interfaces in files using the .cppm extension
> 
> Implement modules in files using the .cpp extension

### Module unit structure

A module interface or implementation unit has a well-defined structure. It contains:
- Global module fragment
- Named module fragment

The **global module** fragment is the portion of the unit where global module symbols may be
included (via headers), declared and/or defined therein.
Those symbols are said to be in the _global module purview_ and may be used in the entire unit,
but will not be exported themselves unless an explicit export declaration is made in the named module fragment.

As an example, one may `#include <vector>` in the global module fragment, and this means
that `std::vector` will now be in the global module purview and usable across the entire module
unit, making it also possible to export declarations with it. However, the `std::vector` template
itself will not be exported through the named module. Any client code that imported this module
will still not know any symbols for `std::vector` unless it also does `#include <vector>`
alongside the import. And the same goes for macros.

For this reason, it is heavily recommended that headers are wrapped in a module which is as low
in the hierarchy as possible, in order to avoid obfuscated header dependencies creeping up at the
top. In particular, one should avoid exporting declarations containing STL header symbols that
are not properly wrapped.

> **Do not leak global symbols or preprocessor macros through a named module interface**

The **named module** fragment is the portion of the unit where the module declaration is exported
and/or implemented. Exported symbols are prefixed by the `export` keyword. Entire structs, classes
or namespaces can be exported in one go.

_Example of a module interface unit_

```cpp
// Global module fragment starts here
module; 

#include <cstdint>
// ... all traditional source/header C++ code goes in here

// Named module fragment starts here
export module example;
// transitive import: expose everything from module "stuff" through "example"
export import stuff;

// foobar() is part of the example module, and exported
export void foobar();

export namespace foo {

// All declarations inside this namespace are exported

using uint32 = unsigned int;

}
```

_Example of a module implementation unit_

```cpp
// Global module fragment starts here
module; 

#include <cstdint>
// ... all traditional source/header C++ code goes in here

// Named module fragment starts here (note: not exported)
module example;
import counterexample; // use anything from module "counterexample" only in here

// private_foobar() is not exported
void private_foobar()
{ /* */ }

// foobar() is implemented in here
void foobar()
{ /* */ }
```