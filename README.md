# Toy Scripting Language written in C99

## Bifrost Script is a small, dynamically typed, Object Oriented scripting language

The syntax is very similar to other imperative programming languages such as Lua and Wren.

### Build Requirements

- Compiler
  - C99 + CRT
  - C++17 (Optional helper header for easier binding)

### Example Program

```swift
import "std:io" for print;

//
// Calculates the n-th fibonacci number using recursion.
//
func fibonacci(n)
{
  if (n < 2)
  {
    return n;
  }

  return fibonacci(n - 1) + fibonacci(n - 2);
}

for (var i = 0; i < 10; i = i + 1)
{
  print("Fibonacci of " + i + " is " + fibonacci(i));
}
```

### Introduction

- The runtime and compiler is very small.
  - Written in portable C99
  - No dependencies besides the C runtime library.
  - Single pass compiler for fast compilation into a simple byte-code format.
- Easy to embed into larger applications.
  - Simple to use C99 API
  - Templated C++17 API for binding of lambdas and other function objects.

### Keywords

These are the 17 keywords used in the language. They can never be used as a variable name.

|         |         |          |
|:-------:|:-------:|:--------:|
| `true`  | `false` | `return` |
| `if`    | `else`  | `for`    |
| `while` | `func`  | `var`    |
| `nil`   | `class` | `import` |
| `break` | `new`   | `static` |
| `as`    | `super` |          |

## Comments

Both C and C++ style comments are supported:

```c
// This is a single line style comment.

/*
  This
    is a
        multi-line
                comment!
*/
```

*Like C the C-style comment cannot be nested by another C-style comment for ex ``/* /* */ */`` is illegal*

## Declaring variables

* Since this is a dynamically typed language to declare a variable of any type you use the 'var' keyword.
* To assign the variable to a default value you can use the '=' sign after the variable name otherwise put a semicolon.
  - Any variable without an initializer will start off as 'nil'.

```
var my_string = "Hello World";
var thisIsInitializedAsNil;
```

## Functions

To declare a function start off with the ``func`` keyword.

```swift
func myFunc(paramA, paramB)
{
  // Code Here...
}
```

## Classes

```swift
class MyClass
{

}
```

### Member Fields

```swift

```

### Constructors and Finalizers

```swift

```

### Statics

```swift

```

### Class Operator Overloading

```swift

import "std:ds" for Array;

class Matrix2x2
{
  var _data;

  func ctor()
  {
    self._data = new Array(
      1.0, 2.0,
      3.0, 4.0
    );
  }

  // Index Operator Getter
  func [](x, y)
  {
    return self._data[x + y * 2];
  }

  // Index Operator Setter
  func []=(x, y, value)
  {
    self._data[x + y * 2] = value;
  }

  // Function call operator
  func call()
  {
    print("[" + self._data[0] + ", " + self._data[1] + "]");
    print("[" + self._data[2] + ", " + self._data[3] + "]");
  }
}

var my_mat = new Matrix2x2()

print(my_mat[0, 1])

my_mat[0, 1] = 2.0

print(my_mat[0, 1])

my_mat();
```

### Inheritance


```swift
class SuperClass
{

}

class SubClass : SuperClass
{

}

```

## Instantiating Objects

To create objects you must use the 'new' operator.

```swift
var instance = new ClassName;
```

* This will NOT call the constructor of the class, to do that you must add some parenthesis to the end of the call.
* To call a custom constructor method just add a '.' then the name of the method otherwise the method named 'ctor' will be called.

```swift
// EX:
var ctorIsCalled       = new ClassName(args...);
var customCtorIsCalled = new ClassName.customCtor(args...);
```

## Importing Modules

```swift

```

## Standard Library

```swift

```
