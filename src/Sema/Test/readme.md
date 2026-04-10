# Cangjie Test Framework

# Cangjie Mock Framework

## Transformations
### Interface methods with default implementation

We want methods with default implementation to behave as all other methods, so we'll be to mock:
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

But the calls  to `foo` are optimized for `A` because it's not overriden and resolved to `I.foo` instead, so there would be no way to mock `A.foo`. To resolve this issue additional transfromations are applied:
1. For interfaces that has methods with default implementation, additional interface is generated, and original is derived from it:
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
2. Classes that implements interface `I` directly or through extend, an imlementation of methods of `I$Buddy` is added:
    ```cangjie
    extend A <: I & I$Buddy {
        func _CN7default1I3fooHv$Buddy(): String {
            // ...
            return this.foo()
        }
    }
    ```
3. Class to original `foo` are replaced with calls to accessor
    ```cangjie
    func bar(a: A): String {
        // ...
        return a._CN7default1I3fooHv$Buddy()
    }
    ```
