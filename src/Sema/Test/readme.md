# Cangjie Mock Framework

## Transformations

### Interface methods with default implementations

Methods with default implementations should behave like all other methods, so they can be mocked:
```cangjie
interface I {
    func foo(): String { "original" }
}

class A {}
extend A <: I {}

func bar(a: A) {
    a.foo() // I.foo
}

// ...
let a = mock<A>()
@On(a.foo()).returns("mocked")
```

However, calls to `foo` are optimized for `A`. Because `foo` is not overridden, these calls are resolved to `I.foo`, so there would be no way to mock `A.foo`.

To resolve this issue, the following transformations are applied:

1. For each interface that has methods with default implementations, an additional interface is generated, and the original interface is derived from it:
    ```cangjie
    interface I$Buddy {
        func _CN7default1I3fooHv$Buddy(): String
    }

    interface I <: I$Buddy {
        // ...
        func _CN7default1I3fooHv$Buddy(): String {
            // ...
            return foo()
        }
    }
    ```
2. For each class that implements interface `I`, either directly or through an extension, implementations for the methods from `I$Buddy` are added:
    ```cangjie
    extend A <: I & I$Buddy {
        func _CN7default1I3fooHv$Buddy(): String {
            // ...
            return this.foo()
        }
    }
    ```
3. Calls to the original `foo` are replaced with calls to the accessor:
    ```cangjie
    func bar(a: A): String {
        // ...
        return a._CN7default1I3fooHv$Buddy()
    }
    ```
