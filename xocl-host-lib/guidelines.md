## Development Guidelines

### Avoid `#define` macros for constants and functions
Use `const` variables, `enum` types, or `constexpr` functions instead.
Macros are only supposed to be used for conditional compilation.

_Example_:
```cpp
// bad
#define BUFFER_SIZE 1024

#define STATE_IDLE 0
#define STATE_BUSY 1

#define MIN(a, b) (a) < (b) ? (a) : (b)

// good
const unsigned BUFFER_SIZE = 1024;

enum State {
    STATE_IDLE,
    STATE_BUSY
};

constexpr int min(int a, int b) {
    return a < b ? a : b;
}

// for conditional compilation
#ifdef DEBUG
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...) while(0)
#endif
```

### Use `nullptr` instead of `NULL` or `0`
`nullptr` is a keyword introduced in C++11 to represent a null pointer. It is a better alternative to `NULL` or `0` because it is type-safe.

_Example_:
```cpp
// bad
int* ptr = NULL;
// good
int* ptr = nullptr;
```

### Use `std::vector` for dynamic memory allocation

_Example_:
```cpp
// bad
int* a = new int[10];
int* b = (int*)malloc(10 * sizeof(int));
delete[] a;
free(b);

// good
std::vector<int> a(10);
std::vector<int> b(10);
/* no need to manually free memory */
```

### Raise exceptions for error handling
Use exceptions to handle errors instead of returning error codes. This makes the code cleaner and easier to read. Make sure to document the exceptions it may throw.

_Example_:
```cpp
// bad
int foo() {
    if (something_is_wrong) return -1;
    return 0;
}

// good
/**
 * @brief example function
 * @throws std::runtime_error if something goes wrong
*/
void foo() {
    if (something_is_wrong) throw std::runtime_error("Something went wrong");
}
```