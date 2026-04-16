# Documentation

We generate API/reference docs with [Doxygen](https://www.doxygen.nl/) using annotated source. Document intent, behavior, constraints, and usage of public types/functions. All Doxygen commands use the `@` prefix.

## Contents

- [Doxygen Commands](#doxygen-commands)
- [Documenting Code](#documenting-code)
  - [Files](#files)
  - [Namespaces](#namespaces)
  - [Classes \& Structs](#classes--structs)
  - [Functions \& Methods](#functions--methods)
  - [Member Variables](#member-variables)
  - [Constants](#constants)
  - [Enums](#enums)
  - [Groups](#groups)
- [Generating Documentation](#generating-documentation)

---

## Doxygen Commands

All commands start with `@`. Common sets:

Identification:
- `@file`  `@namespace`  `@class`  `@struct`  `@enum`

Grouping:
- `@defgroup`  `@ingroup`  `@ref`

Descriptions:
- `@brief`  `@details`  `@note`  `@see`  `@bug`

Parameters / Templates:
- `@param[in]` / `@param[out]` / `@param[in,out]`
- `@tparam`

Returns / Errors:
- `@return`  `@throws`

Misc:
- `@authors`  `@cite`  `@copyright`

---

## Documenting Code

Document public APIs: what it does, constraints, ownership, performance notes (when non-trivial). Omit self-evident commentary.

### Files

Top of each header: brief purpose + optional details/authors/group.

#### Relevant commands

- `@ingroup`
- `@file`
- `@authors`
- `@brief`
- `@details`
- `@note`
- `@see`
- `@copyright`

#### Example

```cpp
/**
 * @ingroup geometry
 * @file polygon.h
 * @authors Alice Brown, Carlos Diaz
 * @brief Defines the Polygon class for representing simple 2-D polygons.
 * @details This file contains the Polygon class, which provides methods for adding vertices, computing area, and checking point containment.
 * @note The Polygon class assumes vertices are provided in counter-clockwise order.
 * @see https://en.wikipedia.org/wiki/Polygon
 * @copyright 2025 Geometry Toolkit
 */
```

### Namespaces

Block above namespace summarizing its responsibility; keep terse.

#### Relevant commands

- `@ingroup`
- `@namespace`
- `@brief`
- `@details`
- `@note`
- `@see`

#### Example

```cpp
/**
 * @ingroup math
 * @namespace constants
 * @brief Defines mathematical constants used throughout the project.
 */
namespace constants {}
```

### Classes & Structs

Block above each: role, invariants, key usage constraints, template parameter meaning.

#### Relevant commands

- `@ingroup`
- `@class`
- `@struct`
- `@brief`
- `@details`
- `@tparam`
- `@note`
- `@bug`
- `@see`

#### Example

```cpp
/**
 * @ingroup geometry
 * @struct Point2
 * @brief Lightweight 2-D point with double precision.
 */
struct Point2 {
    double x {0.0}; ///< X coordinate
    double y {0.0}; ///< Y coordinate
};

/**
 * @ingroup geometry
 * @class Polygon
 * @brief Simple 2-D polygon representation.
 * @details Stores vertices in counter-clockwise order.
 * @note Capacity is fixed at construction.
 * @bug AddVertex does not check for self-intersection.
 */
class Polygon {
public:
    /**
     * @brief Constructs a Polygon with a maximum number of vertices.
     * @param[in] max_vertices  Maximum number of vertices allowed.
     * @throws std::length_error  If max_vertices is zero.
     */
    Polygon(std::size_t max_vertices);

    /**
     * @brief Adds a vertex to the polygon.
     * @param[in] p  New vertex in world coordinates.
     * @return Index of the inserted vertex.
     * @throws std::length_error If max_vertices reached.
     */
    std::size_t Polygon::addVertex(const Point2& p);

    /**
     * @brief Computes the signed area of the polygon.
     * @details Uses the Shoelace formula to compute the area.
     * @note The area is positive if vertices are in counter-clockwise order.
     * @return The signed area of the polygon.
     */
    double area() const;

private:
    std::vector<Point2> vertices_; ///< Vertices in counter-clockwise order.
};
```

### Functions & Methods

Public declarations: brief (first line), params (`@param[in]`, etc.), return, exceptions, noteworthy side-effects.

#### Relevant commands

- `@brief`
- `@param[in]` **or** `param[out]` **or** `param[in,out]`
- `@return`
- `@details`
- `@tparam`
- `@throws`
- `@note`
- `@bug`
- `@see`

#### Example

```cpp
/**
 * @brief Adds a vertex to the polygon.
 * @param[in] p  New vertex in world coordinates.
 * @return Index of the inserted vertex.
 * @throws std::length_error If max_vertices reached.
 */
std::size_t Polygon::addVertex(const Point2& p);
```

### Member Variables

Use `///<` inline for non-obvious fields (units, ownership, ordering). Skip trivial ones.

#### Example

```cpp
std::vector<Point2> vertices_; ///< Vertices in counter-clockwise order.
```

### Constants

Inline `///<` for semantic meaning or units; skip if name is self-descriptive.

#### Example

```cpp
constexpr double kPi = 3.141592653589793;    ///< Circle ratio (radians).
inline constexpr Point2 kOrigin {0.0, 0.0};  ///< Reference point (0,0).
```

### Enums

Block above enum; brief meaning. Inline comment each enumerator unless its name is unambiguous.

#### Relevant commands

- `@ingroup`
- `@enum`
- `@brief`
- `@details`
- `@note`
- `@bug`
- `@see`

#### Example

```cpp
/**
 * @ingroup geometry
 * @enum Axis
 * @brief Principal 3-D axes.
 */
enum class Axis {
    kX, ///< X-axis
    kY, ///< Y-axis
    kZ  ///< Z-axis
};
```

### Groups

Use groups to cluster related APIs. Define large module groups in standalone `.dox` files (e.g., `geometry.dox`) under `docs/` to keep headers lean.

#### Relevant commands

- `@defgroup`
- `ingroup`
- `@brief`
- `@details`
- `@copydoc`
- `@see`

#### Example

```cpp
/**
 * @defgroup geometry Geometry
 * @ingroup math
 * @brief Geometry module for 2D and 3D shapes.
 */
```

---

## Generating Documentation

Configure inputs/outputs in `Doxyfile`; customize layout with `DoxygenLayout.xml`.

Steps:
1. Install Doxygen.
2. From project root run:
    ```bash
    doxygen Doxyfile
    ```
3. Open generated HTML in the output dir configured in `Doxyfile`.
