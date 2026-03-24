# CJMapping 五个处理阶段详细分析

在 [JavaDesugarManager.h:42-43](file:///home/guanjiahui/code/dev/cangjie_compiler/src/Sema/NativeFFI/Java/AfterTypeCheck/JavaDesugarManager.h#L42-43) 中定义了 CJMapping 的五个处理阶段：

```cpp
enum class DesugarCJImplStage : uint8_t { 
    BEGIN, 
    PRE_GENERATE,      // 阶段1: 生成 Lambda/Tuple 胶水代码
    FWD_GENERATE,      // 阶段2: 生成前向类
    IMPL_GENERATE,     // 阶段3: 生成 native 方法
    IMPL_DESUGAR,      // 阶段4: Desugar 数据结构
    TYPECHECKS,        // 阶段5: 类型检查相关 Desugar
    END 
};
```

## 阶段1: PRE_GENERATE - 生成 Lambda/Tuple 胶水代码

### 核心函数
- `PreGenerateInCJMapping()` [DesugarCJMapping.cpp:980](file:///home/guanjiahui/code/dev/cangjie_compiler/src/Sema/NativeFFI/Java/AfterTypeCheck/DesugarCJMapping.cpp#L980)
- `GenerateLambdaGlueCode()` [DesugarCJMapping.cpp:1062](file:///home/guanjiahui/code/dev/cangjie_compiler/src/Sema/NativeFFI/Java/AfterTypeCheck/DesugarCJMapping.cpp#L1062)

### 主要任务
为 **Lambda 表达式** 和 **Tuple 类型** 生成在 Java 和 CJ 之间传递所需的胶水代码。

### 具体实现
1. **初始化 Lambda 工具函数**：
   ```cpp
   void JavaDesugarManager::PreGenerateInCJMapping(File& file)
   {
       if (!isInitLambdaUtilFunc) {
           GenerateLambdaGlueCode(file);
       }
   }
   ```

2. **生成 Lambda 胶水代码**：
   - 为每个 Lambda 模式生成对应的 CJ 函数
   - 生成 Java 可调用的 native 方法
   - 管理 Lambda 对象的生命周期

### 生成的代码示例
```cangjie
// 生成的 CJ Lambda 胶水函数
public func getInt32ToInt32CjLambda(a: jobject) : (Int32) -> Int32 {
    let javaref = Java_CFFI_JavaEntityJobject(a)
    let cjLambda = { b:Int32 =>
        let env = Java_CFFI_get_env()
        let methodId = Java_CFFI_MethodIDConstr(env, Java_CFFI_ClassInit(env, "cj/IntWInt"), "call",
        Java_CFFI_parseMethodSignature("(I)I")); 
        let result = Java_CFFI_callVirtualMethod(env, javaref, methodId,
        [Java_CFFI_JavaEntity(b)], Java_CFFI_JavaCallNestInit(1)) 
        Java_CFFI_unwrapJavaEntityAsValue<Int32>(result)
    }
    cjLambda
}

// 生成的 Native 函数
@C
public func Java_cj_IntWInt_00024BoxIntWInt_intWIntImpl(env: JNIEnv_ptr, _: jclass, self: jlong, arg: Int32): Int32 {
    withExceptionHandling(env) {
        let v = Java_CFFI_getFromRegistry<(Int32) -> Int32>(env, self)
        let r = v(arg)
        r
    }
}
```

---

## 阶段2: FWD_GENERATE - 生成前向类

### 核心函数
- `GenerateFwdClassInCJMapping()` [DesugarCJMapping.cpp:1179](file:///home/guanjiahui/code/dev/cangjie_compiler/src/Sema/NativeFFI/Java/AfterTypeCheck/DesugarCJMapping.cpp#L1179)

### 主要任务
为 CJMapping 的 **接口** 和 **开放类** 生成前向类（Forward Class），作为 Java 和 CJ 之间的桥接层。

### 具体实现
```cpp
void JavaDesugarManager::GenerateFwdClassInCJMapping(File& file)
{
    for (auto& decl : file.decls) {
        if (!decl.get()->TestAttr(Attribute::PUBLIC) || decl.get()->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }
        // 处理接口
        auto interfaceDecl = As<ASTKind::INTERFACE_DECL>(decl.get());
        if (interfaceDecl && IsCJMapping(*interfaceDecl)) {
            GenerateForCJInterfaceMapping(file, *interfaceDecl);
            continue;
        }
        // 处理开放类
        auto classDecl = As<ASTKind::CLASS_DECL>(decl.get());
        if (classDecl && IsCJMapping(*classDecl) && classDecl->TestAttr(Attribute::OPEN)) {
            GenerateForCJOpenClassMapping(*classDecl);
            continue;
        }
    }
}
```

### 生成的前向类示例
```cangjie
class CJMappingInterface_fwd <: CJMappingInterface { 
    // Attribute::CJ_MIRROR_JAVA_INTERFACE_FWD
    public let javaref: Java_CFFI_JavaEntity

    public init(ref: Java_CFFI_JavaEntity) {...}

    public func foo(): Unit {
        jniCall("Java/A", "foo", "()V", [])
    }

    public func foo_default_impl(): Unit {...} 
    // Attribute::JAVA_CJ_MAPPING_INTERFACE_DEFAULT
}
```

---

## 阶段3: IMPL_GENERATE - 生成 native 方法

### 核心函数
- `GenerateInCJMapping()` [DesugarCJMapping.cpp:1199](file:///home/guanjiahui/code/dev/cangjie_compiler/src/Sema/NativeFFI/Java/AfterTypeCheck/DesugarCJMapping.cpp#L1199)

### 主要任务
为各种 CJMapping 类型生成可从 Java 调用的 **native 方法** 和 **构造函数**。

### 具体实现
```cpp
void JavaDesugarManager::GenerateInCJMapping(File& file)
{
    for (auto& decl : file.decls) {
        // ... 过滤条件 ...
        auto structDecl = As<ASTKind::STRUCT_DECL>(decl.get());
        if (structDecl && IsCJMapping(*structDecl)) {
            GenerateForCJStructOrClassTypeMapping(file, structDecl); // 处理结构体
            continue;
        }
        auto enumDecl = As<ASTKind::ENUM_DECL>(decl.get());
        if (enumDecl && IsCJMapping(*enumDecl)) {
            GenerateForCJEnumMapping(*enumDecl); // 处理枚举
            continue;
        }
        auto classDecl = As<ASTKind::CLASS_DECL>(decl.get());
        if (classDecl && IsCJMapping(*classDecl) && !classDecl->TestAttr(Attribute::OPEN)) {
            GenerateForCJStructOrClassTypeMapping(file, classDecl); // 处理普通类
            continue;
        }
        if (classDecl && IsFwdClass(*classDecl)) {
            GenerateNativeForCJInterfaceMapping(*classDecl); // 为前向类生成 native 方法
            continue;
        }
        auto extendDecl = As<ASTKind::EXTEND_DECL>(decl.get());
        if (extendDecl && IsCJMapping(*extendDecl)) {
            GenerateForCJExtendMapping(*extendDecl); // 处理扩展声明
        }
    }
}
```

### 支持的类型
- **结构体** (`struct`)
- **枚举** (`enum`)
- **普通类** (`class`)
- **开放类** (`open class`)
- **接口** (`interface`)
- **扩展声明** (`extend`)

---

## 阶段4: IMPL_DESUGAR - Desugar 数据结构

### 核心函数
- `DesugarInCJMapping()` [DesugarCJMapping.cpp:1243](file:///home/guanjiahui/code/dev/cangjie_compiler/src/Sema/NativeFFI/Java/AfterTypeCheck/DesugarCJMapping.cpp#L1243)

### 主要任务
1. 处理 CJMapping 数据结构的 Desugar
2. 生成对应的 **Java 源代码文件**
3. 处理泛型配置
4. 管理扩展声明

### 具体实现
```cpp
void JavaDesugarManager::DesugarInCJMapping(File& file)
{
    // 映射原始声明到其所有扩展声明
    std::map<Ptr<Decl>, std::vector<Ptr<ExtendDecl>>> ref2extend;
    // 需要生成 Java 胶水代码的原始声明
    std::vector<Ptr<Decl>> genDecls;
    
    // 收集需要处理的声明
    for (auto& decl : file.decls) {
        if (!decl.get()->TestAttr(Attribute::PUBLIC) || decl.get()->TestAttr(Attribute::IS_BROKEN) ||
            !JavaSourceCodeGenerator::IsDeclAppropriateForGeneration(*decl.get()) || !IsCJMapping(*decl.get())) {
            continue;
        }
        if (auto extendDecl = As<ASTKind::EXTEND_DECL>(decl.get())) {
            // 收集扩展声明
        } else {
            genDecls.emplace_back(decl.get());
        }
    }
    
    // 为每个声明生成 Java 代码
    for (auto decl : genDecls) {
        if (IsCJMappingGeneric(*decl)) {
            // 处理泛型配置
            std::vector<GenericConfigInfo*> genericConfigsVector;
            bool isGenericGlueCode = false;
            InitGenericConfigs(file, decl, genericConfigsVector, isGenericGlueCode);
            for (auto& config : genericConfigsVector) {
                const std::string fileJ = config->declInstName + ".java";
                auto codegen = JavaSourceCodeGenerator(decl.get(), mangler, typeManager, javaCodeGenPath, fileJ,
                    GetCangjieLibName(outputLibPath, decl.get()->GetFullPackageName()), config,
                    file.curPackage.get()->isInteropCJPackageConfig);
                codegen.Generate(); // 生成 Java 代码
            }
        } else {
            // 处理非泛型声明
            // ...
        }
    }
}
```

### 关键操作
- **Java 代码生成**: 使用 `JavaSourceCodeGenerator` 生成对应的 Java 类文件
- **泛型处理**: 支持泛型类型的映射
- **扩展管理**: 处理扩展声明的映射

---

## 阶段5: TYPECHECKS - 类型检查相关 Desugar

### 核心函数
- `DesugarTypechecks()`

### 主要任务
处理与类型检查相关的表达式 Desugar，特别是涉及 Java 类型的表达式。

### 具体实现
主要处理以下表达式：
1. **`x is T` 表达式**: 转换为 `Java_CFFI_isInstanceOf` 调用
2. **`x as T` 表达式**: 转换为类型转换和安全检查
3. **匹配模式**: 处理类型匹配模式中的 Java 类型

### 示例处理
```cangjie
// 原始表达式
if (x is JString) {
    // ...
}

// Desugar 后
match (x) {
    case x : JObject =>
        Java_CFFI_isInstanceOf(Java_CFFI_get_env(), x.javaref, "java/lang/String")
    case _ => false
}
```

---

## 处理流程总结

```
┌─────────────────────────────────────────────────────────┐
│ CJMapping 完整处理流程                                    │
└─────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────┐
│ 阶段1: PRE_GENERATE                                      │
│ • 生成 Lambda 胶水代码                                    │
│ • 生成 Tuple 辅助结构                                     │
└─────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────┐
│ 阶段2: FWD_GENERATE                                      │
│ • 为接口生成前向类                                        │
│ • 为开放类生成前向类                                      │
└─────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────┐
│ 阶段3: IMPL_GENERATE                                     │
│ • 生成 native 方法                                        │
│ • 生成构造函数                                            │
│ • 支持 struct/enum/class/interface/extend                 │
└─────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────┐
│ 阶段4: IMPL_DESUGAR                                      │
│ • 生成 Java 源代码文件                                    │
│ • 处理泛型配置                                            │
│ • 管理扩展声明                                            │
└─────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────┐
│ 阶段5: TYPECHECKS                                        │
│ • 处理 is/as 表达式                                       │
│ • 处理类型匹配模式                                        │
└─────────────────────────────────────────────────────────┘
```

## 阶段间依赖关系

| 阶段 | 依赖前序阶段 | 核心输出 | 下一阶段 |
|------|--------------|----------|----------|
| 1 | 无 | Lambda/Tuple 胶水代码 | 2 |
| 2 | 1 | 前向类声明 | 3 |
| 3 | 2 | native 方法和构造函数 | 4 |
| 4 | 3 | Java 源代码文件 | 5 |
| 5 | 4 | Desugar 后的表达式 | 完成 |

---

## 技术要点

1. **对象生命周期管理**: 使用注册表（Registry）管理 CJ 对象在 Java 中的生命周期
2. **JNI 桥接**: 通过 JNI 实现 Java 和 CJ 之间的方法调用
3. **类型映射**: 自动处理 CJ 类型和 Java 类型之间的转换
4. **泛型支持**: 完整支持泛型类型的映射
5. **接口默认方法**: 支持接口默认方法的实现

这些阶段共同构成了完整的 CJMapping 处理流水线，使得 Java 代码能够无缝调用 CJ 编写的类库。