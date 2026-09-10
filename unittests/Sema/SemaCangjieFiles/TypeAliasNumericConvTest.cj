// Test for type alias numeric conversion
// Issue: https://gitcode.com/Cangjie/cangjie_compiler/issues/375
type MyInt64 = Int64
type MyFloat64 = Float64
type MyUInt32 = UInt32
type MyRune = Rune

func testAliasCast() {
    // Test 1: Alias to underlying type (Int64)
    let a: MyInt64 = MyInt64(42)
    let b: Int64 = MyInt64(100)
    
    // Test 2: Alias to Float64
    let c: MyFloat64 = MyFloat64(3.14)
    
    // Test 3: Alias to UInt32
    let d: MyUInt32 = MyUInt32(999)
    
    // Test 4: Alias to Rune
    let e: MyRune = MyRune(65)
}

main() {
    testAliasCast()
    return 0
}