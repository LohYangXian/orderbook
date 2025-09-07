
Some C++ Notes for myself (Written then refined with ChatGPT)

This section explains key C++ concepts:
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

### Understanding `std::shared_ptr` and `OrderPointer`

- `std::shared_ptr` is a smart pointer in C++ that manages the lifetime of a dynamically allocated object.
- Multiple `shared_ptr`s can own the same object; the object is deleted automatically when the last `shared_ptr` owning it is destroyed.

#### Why use `OrderPointer`?

- `OrderPointer` is a type alias for `std::shared_ptr<Order>`.
- It allows you to safely share ownership of `Order` objects across your code (for example, in containers or between functions).
- You don’t need to manually delete the object; memory management is automatic.

#### Summary

- Use `shared_ptr` when multiple parts of your program need to share and manage the same object’s lifetime.
- `OrderPointer` makes your code cleaner and safer by handling memory for `Order` objects.

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

- Use `std::make_shared` to create and manage objects with `std::shared_ptr` easily and