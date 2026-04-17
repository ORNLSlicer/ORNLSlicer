# C++ Style Guide

Follow these guidelines to keep contributions consistent, readable, and maintainable. This document adapts ideas from the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) and the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) for project needs—favor clarity over cleverness.

## Contents

- [Header Files](#header-files)
  - [Include Guards](#include-guards)
  - [Include What You Use](#include-what-you-use)
  - [Include Order](#include-order)
  - [Function Definitions in Headers](#function-definitions-in-headers)
- [Scoping](#scoping)
  - [Namespaces](#namespaces)
  - [Local Variables](#local-variables)
- [Classes](#classes)
  - [Access Control](#access-control)
  - [Declaration Order](#declaration-order)
- [Structs](#structs)
- [Looping and Branching](#looping-and-branching)
- [Preincrement / Predecrement](#preincrement--predecrement)
- [`const` and `constexpr`](#const-and-constexpr)
- [Naming](#naming)
  - [Files](#files)
  - [Namespaces](#namespaces-1)
  - [Classes / Structs](#classes--structs)
  - [Functions / Methods](#functions--methods)
  - [Variables](#variables)
  - [Member Variables](#member-variables)
  - [Constants / Enums](#constants--enums)
  - [Aliases](#aliases)
  - [Templates](#templates)
  - [Macros](#macros)
- [Comments](#comments)
  - [When to Comment](#when-to-comment)
  - [Style](#style)

---

## Header Files

Most `.cpp` files have a matching `.h` for declarations; exceptions: unit tests and tiny `main()` drivers.

### Include Guards

Use `#pragma once` in every header:

```cpp
#pragma once

// other includes or declarations

// file content
```

### Include What You Use

Include the header that defines each symbol you use; avoid relying on transitive includes. Prefer real includes to forward declarations unless you must break dependency cycles.

- Good:

  ```cpp
  #include "a.h" // Include the header that defines A

  class B {
  public:
      void interactWithA(A* a);
  }
  ```

- Bad:

  ```cpp
  class A; // Forward declaration without full definition

  class B {
  public:
      void interactWithA(A* a);
  }
  ```

### Include Order

Order (enforced by `.clang-format`):
1. Related header
2. Standard library
3. Third-party
4. Project headers
Use `""` for project, `<>` for standard/third-party. Update `IncludeCategories` when adding libraries.

### Function Definitions in Headers

Limit header function bodies to templates or short (≤10 lines) helpers. Longer bodies belong in `.cpp` unless required for inlining or templates. If implemented in a header, keep non‑API details in private scope, an internal namespace, or after `// Implementation details below`. Ensure ODR safety (`inline`, template, or in-class).

Example:

```cpp
template <typename T>
class Foo {
public:
    // Short, ODR-safe function defined in header
    int bar() const { return bar_; }

    // Long function declared only; implementation in .cpp or below
    void doSomething();

private:
    int bar_;
};

// Implementation details only below here
template <typename T>
void Foo<T>::doSomething() {
    // ... lengthy implementation ...
}
```

---

## Scoping

Use namespaces and minimal variable scope to avoid collisions and keep intent clear.

### Namespaces

Wrap code in a project/path namespace:

```cpp
namespace my_project {
    // Code goes here
}  // namespace my_project
```

Avoid `using namespace ...;` and inline namespaces.

### Local Variables

Declare variables in the narrowest scope; initialize at declaration:

```cpp
void foo() {
    int x = 42; // Declaration and initialization together.
    // Use x...
}
```

Reuse expensive objects outside loops when safe:

```cpp
Foo f;
for (int i = 0; i < 1000000; ++i) {
  f.doSomething(i);
}
```

---

## Classes

Use `class` for types with behavior; encapsulate invariants.

### Access Control

Keep data `private` (except trivial constants). Add accessors when needed for invariants or semantics.

### Declaration Order

Section order: `public`, `protected`, `private` (skip empties). Inside each group:
1. Types/aliases
2. (Structs) data members
3. Static constants
4. Factories
5. Ctors/assignment
6. Destructor
7. Other functions
8. Other data

Maintain this for predictable navigation.

---

## Structs

Use `struct` for passive aggregates; prefer it over `std::pair`/`std::tuple` when fields have semantic names. Reserve pairs/tuples for generic/meta cases or required API interop.

---

## Looping and Branching

Always use braces for `if/else/for/while/do/switch`, even single-line bodies:

```cpp
if (condition) {
    doSomething();
}
else {
    doSomethingElse();
}

while (condition) {
    doSomething();
}

for (int i = 0; i < 10; ++i) {
    doSomethingWith(i);
}

switch (var) {
    case 0: {
        foo();
        break;
    }
}
```

---

## Preincrement / Predecrement

Prefer prefix (`++i`, `--i`) unless the previous value is required:

```cpp
// Good: preincrement
for (int i = 0; i < n; ++i) {
    std::cout << i << std::endl;
}

// Necessary: postincrement
std::vector<int> vec = {10, 20, 30};
auto it = vec.begin();
while (it != vec.end()) {
    std::cout << *it++ << std::endl; // Value needed before increment
}
```

---

## `const` and `constexpr`

Use `const` for logical immutability; use `constexpr` when the value must be compile‑time. Favor `const` references for parameters you do not modify.

```cpp
void processData(const std::vector<int>& data) {
    constexpr int threshold = 10; // Compile-time constant

    // Use const to ensure data is not modified
    for (const auto& item : data) {
        if (item > threshold) {
            // Do something with item
        }
    }
}
```

---

## Naming

Consistent naming improves scanability. Follow these conventions.

### Files

`snake_case` + extension (`.h`, `.cpp`). Project uses `.h` and `.cpp` only.

### Namespaces

`snake_case` (e.g., `my_project`).

### Classes / Structs

`PascalCase` (e.g., `MyClass`).

### Functions / Methods

`camelCase`; use verbs. Accessors: property name (`eyeColor()`); mutators: `setProperty()`.

#### Example

```cpp
class Dog {
public:
    std::string eyeColor() const { return eye_color_; } // Accessor for eye_color_

    void setEyeColor(const std::string& color) { eye_color_ = color; } // Mutator for eye_color_

private:
    std::string eye_color_;
};
```

### Variables

`snake_case` for locals and parameters.

#### Example

```cpp
void processData(int input_value) {
    int local_variable = input_value * 2;
}
```

### Member Variables

Class members: `snake_case_` (trailing underscore). Struct members: `snake_case` (no underscore).

#### Example

```cpp
class Dog {
private:
    std::string name_;
    int age_;
    std::string eye_color_;
};
```

Use `snake_case` for struct member variables without a trailing underscore.

#### Example

```cpp
struct Point2 {
    double x;
    double y;
};
```

### Constants / Enums

Enums: `PascalCase`. Enum values & constants: `kPascalCase`.

#### Example

```cpp
enum class Color {
    kRed,
    kGreen,
    kBlue
};

const int kMaxValue = 100;
constexpr double kPi = 3.14159;
```

### Aliases

Type aliases: `PascalCase`.

#### Example

```cpp
using StringList = std::vector<std::string>;
```

### Templates

Template parameters: `PascalCase` (e.g., `InputType`).

#### Example

```cpp
template <typename InputType>
InputType processInput(InputType input);
```

### Macros

`UPPER_CASE` with project prefix; prefer `constexpr`/`inline` instead.

#### Example

```cpp
#define MYPROJECT_MAX(a, b) ((a) > (b) ? (a) : (b))
```

> [!NOTE]
> Try to avoid using macros whenever possible. Macros can lead to hard-to-debug issues and are generally discouraged in modern C++. Instead, prefer `inline` or `constexpr` functions.

---

## Comments

Write comments to explain intent, constraints, invariants, and non-obvious decisions—not to restate code.

### When to Comment

Comment when:
- Non-obvious algorithmic trick or tradeoff
- Invariant, pre/post-condition, or subtle side effect
- Temporary workaround or documented TODO
- Rationale (the "why") is not obvious from code

Avoid comments that:
- Restate code literally
- Could be replaced by clearer naming
- Are outdated or speculative

### Style

- Place above the line/block (not trailing inline for long statements)
- Capitalize first word; end with period for full sentences
- Use `TODO(name, YYYY-MM-DD):` for tasks with owner & target date
- Wrap long comments with repeated `//`
- Prefer present tense & active voice
- Be precise; avoid vague statements like "Handle stuff"

```cpp
// Compute signed area via Shoelace formula.
double area = polygonSignedArea(verts);

// Guard: polygon must be simple (no self-intersections).
if (hasSelfIntersection(verts)) {
    return Err::kInvalid;
}

// TODO(alice, 2025-07-01): Replace O(n^2) intersection test with sweep-line algorithm.
```
