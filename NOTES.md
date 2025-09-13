
## Some C++ Notes for myself 
Written then refined with ChatGPT

## Pointers, References and Aliases
- The asterisk (`*`) denotes pointers.
- The ampersand (`&`) denotes references.
- The `using` keyword is used to create type aliases.

Sample code:

```cpp
int value = 42;

// Pointer
int* ptr = &value;

// Reference
int& ref = value;

### When to Use Pointer vs Reference

- **Use a pointer** when:
    - You need to represent "no object" (nullability).
    - The object being pointed to may change (reseating).
    - You need dynamic memory management.

- **Use a reference** when:
    - You want guaranteed access to a valid object (cannot be null).
    - Simpler syntax is preferred.
    - The reference should always refer to the same object.

// Type alias using 'using'
using IntPtr = int*;
IntPtr anotherPtr = &value;
```

### Understanding `std::shared_ptr`

- `std::shared_ptr` is a smart pointer in C++ that manages the lifetime of a dynamically allocated object.
- Multiple `shared_ptr` can own the same object; the object is deleted automatically when the last `shared_ptr` owning it is destroyed.

### Understanding `std::make_shared`

- `std::make_shared<T>(...)` is a C++ function that creates a new object of type `T` and returns a `std::shared_ptr<T>` managing it.
- It combines object creation and smart pointer management in one step.
- It is safer and more efficient than using `new` with `std::shared_ptr` directly.

#### Example

```cpp
auto ptr = std::make_shared<Order>(OrderType::GoodTillCancel, 123, Side::Buy, 100, 10);
// 'ptr' is a shared pointer to a new Order object
```

#### Benefits

- Reduces risk of memory leaks.
- More efficient memory allocation.
- Exception-safe: ensures the object is deleted if an exception occurs during construction.

#### Summary

- Use `std::make_shared` to create and manage objects with `std::shared_ptr` easily

### Understanding `auto&`

auto lets the compiler automatically deduce the type of a variable.
& makes it a reference to the deduced type.

auto& is commonly used in range-based for loops or structured bindings to avoid copying and to work directly with the original object.



### Trading Concepts
#### Fill and Kill Order

- Fill and Kill is an order type used in trading systems.

Fill and Kill means:

- The order will be filled immediately as much as possible, and any unfilled portion is canceled right away.

- It does not stay on the orderbook waiting for future matches.

- Example:
If you place a Fill and Kill order to buy 100 shares, but only 60 are available, you get 60 shares and the remaining 40 are canceled.