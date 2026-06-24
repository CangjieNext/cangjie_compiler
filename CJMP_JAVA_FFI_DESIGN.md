# CJMP Java FFI Interop — Technical Design Document

## Overview

This document describes the changes in commit `c8636f0c` (feat(cjmp): allow common/specific with Java FFI)
which enables the CJMP (common/specific) compilation model to work with Java and ObjC FFI interop.

**Reference documentation**: This document is grounded in the compiler reference docs at
`cjcompiler_reference/doc/`. Cross-references use `[REF:filename]` notation.
Key references:
- Compiler pipeline: [REF:Sema/Sema.md]
- CJMP architecture: [REF:Sema/CJMP/MPTypeCheckerImpl_ARCHITECTURE.md]
- CJMP common/specific: [REF:Sema/CJMP/CJMP_COMMON_SPECIFIC.md]
- CJMP attributes/links: [REF:Sema/CJMP/CJMP_ATTRIBUTES_AND_LINKS.md]
- CJMP pair tracking in CHIR: [REF:Sema/CJMP/CJMP_PAIR_TRACKING_CHIR.md]
- Java FFI: [REF:Sema/JAVA_FFI_JAVAIMPL.md]
- FFI overview: [REF:Sema/SEMA_FFI_OVERVIEW.md]
- Desugaring: [REF:Sema/DESUGAR_COMPREHENSIVE_LIST.md]
- CHIR: [REF:CHIR/CHIR.md]
- AST2CHIR: [REF:CHIR/AST2CHIR.md]
- Canonicalization: [REF:CHIR/CANONICALIZATION.md]

### CJMP Compilation Model

The CJMP model compiles a single source program into two parts
([REF:CJMP_COMMON_SPECIFIC.md:16-25]):
- **Common part** — platform-independent logic, compiled to RAW CHIR + CJO
  (`compileCommon=true`, `outputMode=CHIR`)
- **Specific part** — platform-specific declarations, compiled against the common CJO
  (`compilePlatform=true`)

Common compilation stops at RAW CHIR output — canonicalization, VTable generation,
field path resolution, closure conversion, and CodeGen are all deferred to specific
compilation ([REF:CJMP_COMMON_SPECIFIC.md:516-803]).

### The Core Challenge

For Java FFI, the specific part may use `@JavaMirror`, `@JavaImpl`, or ObjC equivalents,
which have specialized compilation stages ([REF:JAVA_FFI_JAVAIMPL.md:370-660]).
The common part has no knowledge of these annotations — its members are plain cangjie
declarations that get merged with specific declarations during CJMP deserialization
and type-checking.

**Common-part members flow into specific @JavaMirror/@JavaImpl classes without FFI
attributes or type information, breaking FFI-specific compilation stages that assume
all members have proper annotations and bodies.**

This challenge has two dimensions:
1. **AST-level**: Common members lack FFI attributes (`JAVA_MIRROR`, `JAVA_IMPL`),
   have wrong parent class (`Object` instead of `JObject`), and may have null
   `funcBody->body` (for constructors whose bodies are already in RAW CHIR)
2. **CHIR-level**: Common-part bodies are in RAW CHIR with symbolic references
   (`GetElementByName`, `Invoke` with `FuncSigInfo`). These need platform-specific
   patching during canonicalization that the standard deferred stages don't handle

---

## Compilation Pipeline — Where CJMP Java FFI Changes Fit

The Cangjie compiler uses a three-phase Sema pipeline
([REF:Sema.md:24-55]):
```
Phase 1: PreTypeCheck (Preparation & Pre-check)
  ├── Scope/Lookup → PreCheck (Decl Resolution)
  ├── CJMP merge (PrepareTypeCheck4CJMP)
  ├── FFI PrepareTypeCheck (InsertMirrorVarProp, etc.)
  └── PreCheck4CJMP

Phase 2: TypeCheck (Main Type Checking)
  ├── Type Inference (Constraint Solve)
  ├── Pattern Matching (Exhaustiveness)
  └── FFI TypeCheck (Inheritance validation)

Phase 3: PostTypeCheck (Post-validation + Desugaring)
  ├── LegalityOfUsage (12 Checkers)
  ├── DesugarAfterTypeCheck (general)
  ├── Java FFI 6 sub-stages
  ├── ObjC FFI desugaring
  └── ValidateCJMPConstructorConstraints
```

The Java FFI pipeline itself has 6 sub-stages within PostTypeCheck
([REF:JAVA_FFI_JAVAIMPL.md:629-660]):
```
DesugarJavaMirrorImplStage:
  MIRROR_GENERATE_STUB  → Stage 5.1 (insert javaref, ctor stub, finalizer)
  MIRROR_GENERATE       → Stage 5.2 (fill ctor body, array accessors)
  IMPL_GENERATE         → Stage 5.3 (generate @JavaImpl native methods)
  MIRROR_DESUGAR        → Stage 5.4 (desugar @JavaMirror method bodies → JNI)
  IMPL_DESUGAR          → Stage 5.5 (desugar @JavaImpl constructors, super calls)
  TYPECHECKS            → Stage 5.6 (desugar is/as/match expressions)
```

Our changes are organized by pipeline phase below.

---

### Phase 1: PreTypeCheck

#### 1a. CJMP Merge (PrepareTypeCheck4CJMP)

This is where common and specific declarations are matched and merged
([REF:CJMP_COMMON_SPECIFIC.md:59-82], [REF:MPTypeCheckerImpl_ARCHITECTURE.md:27-31]).

```
PrepareTypeCheck4CJMP → MergeCJMPNominalsExceptExtension
  → MergeCommonIntoSpecific for each matched pair
  → Set specificImplementation pointer
  → Mark common as doNotExport = true
```

**Our changes in `CheckCJMP.cpp`:**

1. **Object-to-PlatformParent relaxation** (`MatchCommonNominalDeclWithSpecific`):
   - The reference docs identify `Object != JObject` as a known problem
     ([REF:CJMP_COMMON_SPECIFIC.md:497-513])
   - Common class implicitly extends `Object` (cangjie default)
   - Specific `@JavaImpl` class gets `JObject` auto-inserted by PreCheck
     ([REF:JAVA_FFI_JAVAIMPL.md:349-362])
   - This creates `Object→JObject` parent mismatch in `MatchSuperTypes`
     ([REF:CJMP_COMMON_SPECIFIC.md:240-269])
   - **Solution**: `IsPlatformParent` recognizes JAVA_IMPL/OBJ_C_IMPL/JAVA_MIRROR/OBJ_C_MIRROR
     as valid platform parent annotations, allowing the relaxation
   - Relaxations stored in `pendingRelaxations` and validated later

2. **Member matching for FFI classes**:
   - Common VarDecl can match specific PropDecl (created by `InsertMirrorVarProp`)
   - This is a cross-kind match — `CheckMatchedVariableTypes` in reference docs
     ([REF:CJMP_COMMON_SPECIFIC.md:286-297]) only covers VarDecl→VarDecl
   - `CheckReturnAndVariableTypes` handles VarDecl→PropDecl type comparison
     using `DESUGARED_MIRROR_FIELD` attribute as the structural signal

3. **Annotation filtering** (`CheckCJMPAnnotations.cpp`):
   - `PlatformSpecificAnnotations` set includes JAVA_IMPL, JAVA_MIRROR, OBJ_C_MIRROR,
     JAVA_HAS_DEFAULT, OBJ_C_INIT, OBJ_C_OPTIONAL
   - These annotations are excluded from common/specific matching
     ([REF:CJMP_COMMON_SPECIFIC.md:218-231]) — they exist only in the specific part
   - Extends the documented `MatchCJMPDeclAttrs` algorithm to recognize that
     FFI annotations are platform-specific and should not trigger mismatch errors

#### 1b. PreCheck

```
PreCheck Phase ([REF:Sema/PRECHECK_PHASE.md]):
  ├── ResolveNames(ctx)
  ├── BuildExtendMap(ctx)
  ├── AddSuperClassObjectForClassDecl(ctx) ← JOBJECT ADDED HERE!
  │   ├── For JAVA_MIRROR/JAVA_MIRROR_SUBTYPE:
  │   │   └── AddJObjectSuperClassJavaInterop(cd)
  │   └── For regular classes:
  │       └── AddObjectSuperClass(cd)
  ├── PreCheck4CJMP(pkg)
```

**Our changes:**
- `PreCheck.cpp`: `IsCJMPCommonSpecificPair` — prevents common/specific constructor
  pairs from being flagged as redefinition conflicts. Without this, the common `init()`
  and the specific `init()` (inherited via CJMP merging) would trigger "duplicate
  definition" errors.

**Context**: JObject is added AFTER CJMP merge but BEFORE CJMP matching validation
([REF:JAVA_FFI_JAVAIMPL.md:563]). This ordering is critical — it means `Object→JObject`
relaxation must be handled in the matching phase, not earlier.

#### 1c. PrepareTypeCheck — Java FFI Preparation

```
JavaInteropManager::PrepareTypeCheck(pkg, importManager, typeManager)
  ([REF:JAVA_FFI_JAVAIMPL.md:419-447]):
  → InsertMirrorVarProp (Var→Prop conversion for @JavaMirror fields)
  → AssignMirrorPropGetterSetterTypes (assign function types) [NEW]
  → RemoveAbstractAttributeForJavaHasDefaultMethods
  → InsertJavaHasDefaultMethodStubs
```

This runs AFTER CJMP merge ([REF:JAVA_FFI_JAVAIMPL.md:449]).
Common members are already present in the specific class when this stage runs.

**Our changes:**

1. **`AssignMirrorPropGetterSetterTypes`** (new function in `NativeFFI/Java/BeforeTypeCheck/Utils.cpp`):
   - After `InsertMirrorVarProp` converts VarDecl→PropDecl ([REF:JAVA_FFI_JAVAIMPL.md:453-533]),
     the generated getters/setters lack function types
   - This function assigns proper function types:
     - Getter: `GetFunctionTy({}, pd.GetTy())` — returns property type
     - Setter: `GetFunctionTy({pd.GetTy()}, TYPE_UNIT)` — takes property type, returns Unit
     - Setter parameter: `setterParam->SetTy(pd.GetTy())`
   - Only processes properties with `DESUGARED_MIRROR_FIELD` attribute and valid `pd.GetTy()`
   - Called in `GenerateJavaMirror.cpp::PrepareTypeCheck` right after `InsertMirrorVarProp`
   - **Why needed**: CJMP deserialized common VarDecls may have `pd.GetTy()` as `InitialTy`
     until TypeCheck resolves it. Without this, setter parameters crash in ASTTypeValidator
     and CreateJavaEntityCall.

2. **`InsertPropGetterSignature` / `InsertPropSetterSignature`** (in `AST/Utils.cpp`):
   - Added position info (`begin/end/curFile`) for generated FuncDecls and FuncBodies
   - For setters: `setterParam->type = ASTCloner::Clone(prop.type)` — provides syntactic
     type for the setter parameter (needed for type-checking stages)
   - **Layer alignment**: These remain in AST layer ([REF:SEMA_FFI_OVERVIEW.md:167-190]),
     not Sema — consistent with documented shared infrastructure pattern

3. **`InsertMirrorVarProp` rewrite** (in `AST/Utils.cpp`):
   - Previously: collect all VarDecls, convert to PropDecls, remove VarDecls
   - Now: uses `varToPropMap` and `transformedIndices` to track which VarDecl becomes
     which PropDecl, then calls `LinkMatchedCommonPropToSpecific` to link matched pairs
   - `LinkMatchedCommonPropToSpecific`: sets `specificImplementation` on common PropDecl
     getters/setters pointing to their specific counterparts
   - **Extends documented pattern**: The FFI filtering pattern
     ([REF:CJMP_ATTRIBUTES_AND_LINKS.md:74-86]) uses `FROM_COMMON_PART + specificImplementation`
     to skip merged members. Our linking enables this pattern for getter/setter pairs too.

4. **`InsertPropConvertedByField`** signature change:
   - Added `PropDecl** propPtr` parameter — stores pointer to created PropDecl
   - Added `curFile` field assignment for proper file context
   - Enables `InsertMirrorVarProp` to track PropDecl creation for later linking

---

### Phase 2: TypeCheck

```
TypeCheck Phase ([REF:Sema.md:37-43]):
  ├── Type Inference (Constraint Solve)
  ├── Pattern Matching (Exhaustiveness)
  ├── FFI TypeCheck (Inheritance validation)
  └── MatchSpecificWithCommon (CJMP validation)
```

**Our changes:**

1. **`AddReturnTypeForPropMemDecl`** (`TypeChecker.cpp`):
   - Changed `!Ty::IsCorrect(pd.GetTy())` to `!pd.type` for setter skip condition
   - For freshly parsed VarDecls, `pd.GetTy()` is `InitialTy` at BeforeTypeCheck —
     the `IsTyCorrect` check incorrectly skips setters whose semantic type hasn't
     been resolved yet. The `pd.type` check correctly uses syntactic type availability
     as the condition.
   - **Why**: CJMP deserialized VarDecls have `pd.GetTy()` as `InitialTy` until TypeCheck
     resolves it. The `IsTyCorrect` check rejects `InitialTy`, causing setters to be
     skipped. Using `pd.type` (syntactic type) as the condition allows TypeCheck to
     fill in `funcBody->retType` even when `pd.GetTy()` is still `InitialTy`.

---

### Phase 3: PostTypeCheck

#### 3a. CJMP Post-Validation

```
PostTypeCheck (CJMP-specific):
  → CheckReturnAndVariableTypes (validate type compatibility)
  → ValidateMatchedAnnotationsAndModifiers
  → ValidateCJMPConstructorConstraints [NEW]
```

**Our changes:**

- `ValidateCJMPConstructorConstraints` (called from `TypeChecker::PostTypeCheck`):
  - Validates that platform parent has an empty constructor
  - Diagnoses `sema_cjmp_no_empty_ctor_in_platform_parent` if validation fails
  - **Why**: CJMP @JavaImpl constructors need to call platform parent's empty
    constructor via CHIR-level `PatchCJMPSuperCalls`. If the platform parent
    (e.g., JObject) lacks an empty constructor, the CHIR patching would fail
    at runtime.

#### 3b. Java FFI Desugaring — 6 Sub-Stages

The Java FFI desugaring runs as 6 sub-stages within PostTypeCheck
([REF:JAVA_FFI_JAVAIMPL.md:629-660]):

##### Sub-stage 5.1: MIRROR_GENERATE_STUB

```
GenerateInMirrors(file, doStub=true)
  ([REF:JAVA_FFI_JAVAIMPL.md:644-667]):
  → InsertJavaRefVarDecl (javaref field)
  → InsertJavaMirrorCtor (constructor stub)
  → InsertJavaMirrorFinalizer (finalizer stub)
  → InsertJavaMirrorHasInited (hasInited flag)
```

**Our changes:**
- `COMPILER_ADD` attribute additions on `javaref` and `hasInited` fields
  - `javaref` field: `JAVA_MIRROR + COMPILER_ADD`
  - `hasInited` field: `PRIVATE + NO_REFLECT_INFO + IN_CLASSLIKE + HAS_INITED_FIELD + COMPILER_ADD`
  - **Pattern**: Consistent with documented `COMPILER_ADD` usage
    ([REF:JAVA_FFI_JAVAIMPL.md:316]): "Marks compiler-added types (e.g., JObject superclass)"
  - **Why**: CJMP deserialized fields lack syntactic type information.
    `COMPILER_ADD` prevents AST-level validators from complaining.

##### Sub-stage 5.4: MIRROR_DESUGAR

```
DesugarMirrors(file)
  ([REF:JAVA_FFI_JAVAIMPL.md:706-726]):
  → DesugarJavaMirrorConstructor (JNI: Java_CFFI_newObject)
  → AddJavaMirrorMethodBody (JNI: Java_CFFI_callMethod_raw)
  → InsertJavaMirrorPropGetter (JNI: Java_CFFI_getField)
  → InsertJavaMirrorPropSetter (JNI: Java_CFFI_setField)
```

**Our changes in `DesugarJavaMirror.cpp`:**

1. **CJMP validation guards** (two-tier pattern):
   - **Matched members** (`FROM_COMMON_PART + specificImplementation`): skip entirely
     — they're handled by the specific implementation. This follows the documented
     FFI filtering pattern ([REF:CJMP_ATTRIBUTES_AND_LINKS.md:74-86]).
   - **Unmatched members** (`FROM_COMMON_PART, no specificImplementation`): diagnose error
     and mark IS_BROKEN. Common constructors in @JavaMirror are invalid — they lack
     JAVA_MIRROR attribute and their body references common-part-only variables that
     can't be desugared into JNI calls.
   - Diagnostics: `sema_cjmp_mirror_ctor_from_common_has_body`,
     `sema_cjmp_mirror_method_from_common_has_body`, `sema_cjmp_mirror_prop_from_common`
   - Also skip IS_BROKEN members at top of iteration loop

2. **Expression-based desugaring compatibility** ([REF:SEMA_FFI_OVERVIEW.md:316-347]):
   - Java FFI uses expression-based approach (`expr.desugarExpr`)
   - CJMP FROM_COMMON_PART members can't have their expressions desugared because
     their bodies are already in RAW CHIR
   - Solution: skip desugaring entirely for FROM_COMMON_PART members in @JavaImpl
     and diagnose/mark IS_BROKEN for unmatched members in @JavaMirror

##### Sub-stage 5.5: IMPL_DESUGAR

```
DesugarInJavaImpls(file)
  ([REF:JAVA_FFI_JAVAIMPL.md:800-827]):
  → DesugarJavaImpl(decl) (JNI object creation + registration)
  → DesugarJavaImplConstructor (super call → Java_CFFI_newObject)
  → GenerateNativeMethod (native method declarations)
```

**Our changes in `DesugarJavaImpl.cpp`:**
- Skip members with `FROM_COMMON_PART` whose `outerDecl` has COMMON or SPECIFIC attribute
- Common-part members in @JavaImpl don't need impl-specific desugaring — their bodies
  are from common (already in RAW CHIR, no JNI calls needed)
- `GenerateInJavaImpl`: same skip pattern
- **Critical constraint**: FROM_COMMON_PART constructors in @JavaImpl have
  `funcBody->body == null` because their bodies are already translated to RAW CHIR
  in the common compilation. The AfterTypeCheck pipeline stages that access
  `funcBody->body` must handle null gracefully.

#### 3c. ObjC FFI Desugaring (Parallel to Java)

ObjC FFI follows the same FFI architecture pattern ([REF:SEMA_FFI_OVERVIEW.md:94-98])
with `@ObjCMirror`/`@ObjCImpl` instead of `@JavaMirror`/`@JavaImpl`.

**Our changes** (mirror the Java pattern):
- `DesugarImpls.cpp`: skip `FROM_COMMON_PART + specificImplementation` members
- `DesugarMirrors.cpp`: skip `FROM_COMMON_PART + specificImplementation` members
- `ASTFactory.cpp`: `COMPILER_ADD` on `nativeHandle` and `hasInited` fields

#### 3d. Java Source Code Generation

```
JavaSourceCodeGenerator ([REF:SEMA_FFI_OVERVIEW.md:406-412]):
  → AddConstructors / AddMethods / AddInstanceMethod
```

**Our changes:**

1. **`FROM_COMMON_PART + specificImplementation` skip**:
   - In `AddConstructors` and `AddMethods`: skip matched common members
   - These members already have their specific-side implementations — generating
     Java source for the common version would create duplicate Java-side declarations
   - Same FFI filtering pattern ([REF:CJMP_ATTRIBUTES_AND_LINKS.md:74-86])

2. **Parameter type validation** (`Ty::IsTyCorrect` checks):
   - Before `AddInstanceMethod/AddStaticMethod`: validate that parameter types are
     resolved (`param->type && Ty::IsTyCorrect(param->type->GetTy())`)
   - CJMP members may have unresolved types (`InitialTy` or `theInvalidTy`) from
     deserialization — attempting to map these to Java types crashes the generator

---

### CHIR Phase: AST2CHIR + Canonicalization

This is where CJMP-specific transformations happen that can't be done at AST level
(because common-part bodies are already translated to RAW CHIR).

#### RAW vs Canonicalized CHIR — Fundamental Context

The reference docs establish that CJMP common compilation produces RAW CHIR
([REF:CJMP_COMMON_SPECIFIC.md:516-803], [REF:CHIR.md#cjmp-deferred-compilation]):
- RAW CHIR uses **symbolic references**: `GetElementByName` (string names),
  `Invoke` with `FuncSigInfo` (method name + signature, no VirMethodOffset)
- Canonicalized CHIR uses **concrete indices**: `GetElementRef` (uint64_t offsets),
  `Invoke` with baked `VirMethodOffset` annotation

Common compilation stops at RAW CHIR. Specific compilation loads the common CJO
(deserializes RAW CHIR), then runs canonicalization which converts symbolic→concrete
([REF:CJMP_COMMON_SPECIFIC.md:560-636]):

```
Canonicalization passes ([REF:CANONICALIZATION.md]):
  Pass 1: GenerateVTable (VTable layout, VirMethodOffset baking)
  Pass 2: MarkClassHasInited (finalizer guard flag)
  Pass 3: UpdateMemberVarPath (GetElementByName → GetElementRef)
  Pass 4: NoSideEffectMarker (pure function hints)
  Pass 5: BoxRecursionValueType (recursive value type boxing)
```

**FFI compatibility with RAW phase** ([REF:CJMP_COMMON_SPECIFIC.md:1149-1177]):
JNI uses symbolic names (`Java_CFFI_getField(env, javaref, "fieldName")`),
matching RAW CHIR's symbolic representation. No field indices or VTable offsets needed.
This is why @JavaMirror desugaring works in RAW phase — JNI calls use string-based
field/method access, not concrete indices.

#### Why CJMP + Java FFI Needs CHIR-Level Patching

FROM_COMMON_PART constructors in @JavaImpl have their bodies in RAW CHIR, not in AST.
The AST `funcBody->body` is null. This means:
- **AST-level desugaring can't modify these bodies** — they don't exist at AST level
- **The bodies already contain `Apply { IsSuperCall: true, callee: Object.init() }`**
  (from common compilation where the parent is `Object`)
- **In the specific @JavaImpl class, the parent is `JObject`**, not `Object`
- **The super call target must be patched from `Object.init()` to `JObject.init()`**

This patching can only happen at CHIR level, where the RAW CHIR body is available
for modification during canonicalization.

Similarly, @JavaMirror field access in common-part methods needs patching:
- Common methods reference `this.fieldName` directly (VarDecl)
- In @JavaMirror, fields are converted to PropDecl with JNI getter/setter access
- Field load/store instructions need to be replaced with getter/setter calls

#### AST2CHIR Translation Phase

```
AST2CHIR pipeline ([REF:AST2CHIR.md:34-69]):
  Phase 1: Collection → Phase 2: Top-Level Caching → Phase 3: Signatures
  → Phase 4: Translation → Phase 5: Initialization → Phase 6: Setup
  → Phase 7: Function Bodies
```

**Our changes:**

1. **`MaybeDeserialized` extension** (in `ASTPackage2CHIR.cpp`):
   - Added `decl.specificImplementation` as a condition for deserialization recognition
   - CJMP matched members have this pointer set by `LinkMatchedCommonPropToSpecific`
   - Without this, matched common members wouldn't be recognized as deserialized
     and would fail CHIR translation

2. **FFI attribute propagation** (in `CreateCustomTypeDef`):
   - Propagates JAVA_MIRROR/JAVA_IMPL/OBJ_C_MIRROR/OBJ_C_IMPL attributes to
     deserialized type definitions in CHIR
   - Sets `FROM_COMMON_PART` attribute on deserialized common members
   - **Why**: The AST-level `FROM_COMMON_PART` attribute must survive into CHIR so
     canonicalization and CodeGen stages can identify CJMP common members

3. **Getter/setter collision handling** (in `CreateFuncSignatureAndSetGlobalCache`):
   - When common and specific both define a getter/setter for the same property:
     - SPECIFIC version replaces the common version (specific wins)
     - Matched common version reuses the SPECIFIC Function (avoids duplicate CHIR functions)
   - **Extends**: The `PopulateCJMPPairs` pattern ([REF:CJMP_PAIR_TRACKING_CHIR.md:52-61])
     tracks common/specific pairs. Our collision handling ensures CHIR Function Values
     are not duplicated for matched getter/setter pairs.

4. **`SkipMemberFuncInSpecificMerging`** (in `TranslateClassDecl.cpp`):
   - Handles getters and setters separately when merging common/specific members
   - Previously treated getter+setter as a single unit; CJMP needs them separate
     because common getter may match specific getter while setter is unmatched

5. **Body translation skip** (in `TranslateFuncDecl.cpp`):
   - Skip body translation for FROM_COMMON_PART constructors
   - These constructors have bodies already translated in the common part RAW CHIR
   - Attempting to translate again would create duplicate CHIR instructions
   - **Context**: This follows the deferred compilation model
     ([REF:CJMP_COMMON_SPECIFIC.md:516-803]) — common bodies are already in RAW CHIR

6. **`PatchCJMPSuperClass`** (in `ASTPackage2CHIR.cpp`):
   - For @JavaImpl classes from common part: patches super class reference
     from `Object` to `JObject` in CHIR ClassDef
   - Runs during AST2CHIR translation (before canonicalization)
   - **Why**: The CHIR ClassDef's `SetSuperClass` must reference the correct platform parent

7. **`PatchCommonFieldsForMirror`** (in `ASTPackage2CHIR.cpp`):
   - For @JavaMirror/@ObjCMirror classes: patches field access in common-part methods
     to use the generated `javaref` field for JNI calls
   - Common methods reference `this.id` directly, but in @JavaMirror, field access
     should go through JNI getter/setter calls
   - Uses `JavaMirrorFieldHelper` to identify which fields need patching
   - **Context**: In RAW CHIR, field access is `GetElementByName(base, ["fieldName"])`
     ([REF:CJMP_COMMON_SPECIFIC.md:830-835]). Our patching replaces this with
     getter/setter `Apply` calls for mirror properties.

8. **`PatchFieldReferencesForMirror`** (in `ASTPackage2CHIR.cpp`):
   - For @JavaMirror global functions that reference mirror class fields
   - Patches CHIR field references to use the correct mirror-specific field accessor

9. **`CHIRFieldReferencePatcher`** (new file):
   - General-purpose CHIR field reference patching utility
   - Walks CHIR instructions, finds field load/store operations, replaces target
   - Used by both `PatchCommonFieldsForMirror` and `PatchFieldReferencesForMirror`

10. **`JavaMirrorFieldHelper`** (new file):
    - Identifies which fields in a @JavaMirror class need CHIR-level patching
    - Maps AST VarDecl names to their mirror PropDecl counterparts
    - Provides field accessor function references for patching

#### Canonicalization Phase — New CJMP Pass

```
Canonicalization ([REF:CANONICALIZATION.md]):
  Pass 1: GenerateVTable
  Pass 2: MarkClassHasInited
  Pass 3: UpdateMemberVarPath (GetElementByName → GetElementRef)
  Pass 4: NoSideEffectMarker
  Pass 5: BoxRecursionValueType
  Pass 6: PatchCJMPSuperCalls [NEW — CJMP + FFI specific]
```

**Our changes:**

1. **`PatchCJMPSuperCalls`** (new file `src/CHIR/Transformation/PatchCJMPSuperCalls.cpp`):
   - For @JavaImpl/@ObjCImpl constructors from common part: patches or inserts
     `super()` call to the platform parent's empty constructor
   - Common constructors contain `Apply { IsSuperCall: true, callee: Object.init() }`
   - Specific @JavaImpl class needs `Apply { IsSuperCall: true, callee: JObject.init() }`
   - **Why in canonicalization**: Super call patching must happen after AST2CHIR
     translation (where RAW CHIR bodies are loaded) and before CodeGen. The
     reference docs show that super-constructor calls use Apply with direct pointer
     ([REF:CJMP_COMMON_SPECIFIC.md:730-759]) — no symbolic reference, so patching
     is a direct Function pointer replacement.
   - **Design concern**: Adding a new pass to canonicalization is a non-standard
     extension. The reference docs list 5 canonicalization passes
     ([REF:CANONICALIZATION.md:16-18]). Our pass runs as an additional step,
     potentially between Pass 3 and Pass 4, or as a separate CHIR transformation
     called from `CHIR.cpp::Canonicalization`.
   - Finds platform parent empty constructor via `FindPlatformParentEmptyConstructor`

---

### CHIR Validation Phase

```
CHIR Checker ([REF:CHIR/CHIR_CHECKER.md]):
  → VarInitCheck (variable initialization analysis)
  → AST2CHIRChecker (IR validation)
```

**Our changes:**
- `VarInitCheck.cpp`: skip deserialized CJMP constructors from variable initialization
  checks for non-SPECIFIC types only. For SPECIFIC types the check must run because
  specific members may be uninitialized even when the constructor body is inherited
  from the common part. (Fix: `func->GetOuterDeclaredOrExtendedDef()->TestAttr(Attribute::SPECIFIC)`)
- `AST2CHIRChecker.cpp`: skip CJMP relaxation-related deserialized classes from
  certain CHIR checks (COMMON/SPECIFIC attributes signal CJMP merging)

---

## Key Design Principles

### 1. Two-tier CJMP member handling

This extends the documented FFI filtering pattern
([REF:CJMP_ATTRIBUTES_AND_LINKS.md:74-86]):
```cpp
for (auto& member : cls->GetMemberDecls()) {
    if (member->TestAttr(Attribute::FROM_COMMON_PART) &&
        member->specificImplementation) {
        continue;  // Matched — handled by specific implementation
    }
    GenerateFFIBinding(member);
}
```

- **Matched members** (`FROM_COMMON_PART + specificImplementation`): skip entirely in
  FFI desugaring — they're handled by the specific implementation
- **Unmatched members** (`FROM_COMMON_PART, no specificImplementation`): diagnose error
  and mark IS_BROKEN — they can't be desugared because they lack FFI attributes

This pattern is applied consistently across all 6 Java FFI sub-stages, ObjC FFI stages,
Java source code generation, and CHIR collision handling.

### 2. AST layer independence from Sema

([REF:SEMA_FFI_OVERVIEW.md:167-190])

- `InsertMirrorVarProp`, `InsertPropGetterSignature`, `InsertPropSetterSignature` are
  in `AST/Utils.cpp` (not Sema) — they don't depend on TypeManager or ImportManager
- `AssignMirrorPropGetterSetterTypes` IS in Sema (needs TypeManager) but is called
  from BeforeTypeCheck, not from AST layer
- `COMPILER_ADD` attribute used instead of Sema-level type checks to protect
  compiler-generated fields from AST validators
- Follows the documented Pimpl pattern ([REF:MPTypeCheckerImpl_ARCHITECTURE.md:64-82])
  and `CANGJIE_CODEGEN_CJNATIVE_BACKEND` conditional compilation

### 3. CHIR-level patching for common-part bodies

This is necessitated by the deferred compilation model
([REF:CJMP_COMMON_SPECIFIC.md:516-803]):

- Common constructor bodies are translated to RAW CHIR in the common compilation
- AST-level desugaring can't modify these bodies (they don't exist at AST level —
  `funcBody->body` is null for FROM_COMMON_PART constructors)
- `PatchCJMPSuperCalls` handles super call patching during canonicalization
- `PatchCommonFieldsForMirror` handles field access patching during AST2CHIR
- `PatchCJMPSuperClass` handles parent class reference patching during AST2CHIR

**This is the fundamental constraint that makes the @JavaImpl pipeline rebase difficult:
FROM_COMMON_PART constructors have no AST body, only RAW CHIR body. Any AfterTypeCheck
stage that accesses `funcBody->body` will encounter null.**

### 4. `specificImplementation` as structural signal

([REF:CJMP_ATTRIBUTES_AND_LINKS.md:36-55], [REF:CJMP_PAIR_TRACKING_CHIR.md:16-21])

- Set by `LinkMatchedCommonPropToSpecific` during `InsertMirrorVarProp`
- Extends the documented `specificImplementation` mechanism (which normally links
  common→specific at class/member level) to also link getter/setter pairs
- Used by CJMP type-checking to recognize matched pairs
- Used by FFI desugaring stages to skip matched common members
- Used by CHIR collision handling to reuse specific functions
- NOT set on unmatched common members or common constructors with bodies
  (which don't need specific counterparts)

### 5. Position info for compiler-generated declarations

- `begin/end/curFile` set on all generated FuncDecls, FuncBodies, FuncParams
- Without this, generated declarations have zero position, causing crashes in
  diagnostics, serialization, and source mapping
- Especially critical for CJMP where generated declarations are deserialized
  and need valid position info for error reporting

### 6. CJMP Merge function behavior

**MergeCommonIntoSpecific** (`src/Sema/CJMP/CheckCJMP.cpp`):

After matching common and specific member declarations, the function:

1. **Swaps matched pairs**: When a specific var matches a common var (by identifier),
   the two are swapped in the merged list. The common var ends up in `specificDecls`
   and the specific var ends up in `mergedDecls`.
2. **Full swap**: `std::swap(specificDecls, mergedDecls)` exchanges the complete lists.
3. **doNotExport on common-matched vars**: Common vars that have a
   `specificImplementation` counterpart are marked `doNotExport = true` to prevent
   `CheckForConflictExportId` from detecting duplicate exportIds (the specific
   counterpart is already in the member list with the same exportId).
4. **Common-only vars preserved**: Common vars WITHOUT `specificImplementation`
   (no specific counterpart) are NOT marked doNotExport — they must remain visible.

**Migration of matched vars**: After the merge, `specificDecls` contains both the
specific var and the swapped-in common var. The common-matched var (with doNotExport)
is skipped by:
- `ASTWriter::CheckForConflictExportId` — checks `doNotExport`
- `ShouldTranslateMember` — returns false for members from different files
- `IsCommonMatchedWithSpecific()` — in `TranslateVariablesInit` skip

### 7. `$varInit` creation and constructor patching

**`$varInit` creation** (`TranslateVarsInit` → `ClearOrCreateVarInitFunc`):

Called from `ProcessClassStructVarInits` during `TranslateNominalDecls`:

1. Finds existing deserialized `$varInit` by mangled name (common part may have it
   if common had vars with initializers)
2. If found: replaces body (clears all old initializations)
3. If not found: creates new `$varInit` function
4. The new body is populated by `TranslateVariablesInit` which processes the AST
   member list of the specific class

**TranslateVariablesInit filtering** (`src/CHIR/AST2CHIR/TranslateASTNode/TranslateFuncDecl.cpp`):

```cpp
for (auto [member, fieldPosition] : initOrder) {
    if (member->IsCommonMatchedWithSpecific()) continue;   // skip matched common
    MemberVarInfo* varInfo = varInfoByName.at(member->identifier);
    if (ShouldTranslateMember(parent, *member)) {
        auto initializerFunc = trans.TranslateVarInit(*member);
        varInfo->initializerFunc = initializerFunc;
    }
    // Add call to init function in $varInit body
    if (varInfo->initializerFunc) { /* create Apply */ }
}
```

- **Common-matched vars**: skipped (specific counterpart handles init)
- **Specific vars**: translated via `TranslateVarInit` — generates per-field init function
- **Common-only vars**: NOT translated (ShouldTranslateMember returns false for
  FROM_COMMON_PART), but the CHIR member retains its initializer from the
  deserialized common CHIR member (preserved by `AddMemberVarDecl` update path)

**Constructor patching** (`TranslateConstructorFunc`):

When the constructor is translated (step 4 of `TranslateAllDecls`), it checks:
```cpp
if (!HasDelegatedThisCall(funcBody)) {
    Function* varInitFunc = typeDef->GetVarInitializationFunc();
    CJC_NULLPTR_CHECK(varInitFunc);
    // Insert Apply(varInitFunc, this) at start of constructor body
}
```

This works for deserialized constructors too because `TranslateFuncDecl` re-translates
the body of a deserialized function: it creates a new entry block, populates it with
the AST body + $varInit Apply call, and replaces the old entry.

**VarInitCheck adjustment**: Deserialized constructors were entirely skipped in
`VarInitCheck::RunOnPackage`, which prevented `chir_class_uninitialized_field` for
SPECIFIC types. Fixed to only skip when the enclosing type is NOT SPECIFIC.

### 8. Remaining gap: constructor-less specific classes

When the specific class has no own constructor (inherits from common), the inherited
deserialized constructor IS re-translated (as above). The `$varInit` call is added
correctly by `TranslateConstructorFunc`. However, if the common class had NO vars
with initializers (so no deserialized `$varInit` existed), the per-field init for
specific-only variables may still be missing from the constructor body.

This manifests in tests like `use_platform_member_define_global_variable` where
`specific let A1:Int32=1` is not initialized (outputs 0 instead of 1), and
`check_types` where the MTable resolution fails at runtime.

---

## Novel Design Elements vs Existing Patterns

### Extending Existing Patterns

| Our Change | Extends | Reference |
|------------|---------|-----------|
| `FROM_COMMON_PART + specificImplementation` skip | FFI filtering pattern | [REF:CJMP_ATTRIBUTES_AND_LINKS.md:74-86] |
| `COMPILER_ADD` on javaref/hasInited | COMPILER_ADD pattern | [REF:JAVA_FFI_JAVAIMPL.md:316] |
| `InsertMirrorVarProp` CJMP linking | Shared FFI infrastructure | [REF:SEMA_FFI_OVERVIEW.md:167-190] |
| `PlatformSpecificAnnotations` | CJMP annotation matching | [REF:CJMP_COMMON_SPECIFIC.md:218-231] |
| `MaybeDeserialized` extension | CJMP pair tracking | [REF:CJMP_PAIR_TRACKING_CHIR.md:52-61] |
| Body translation skip | Deferred compilation model | [REF:CJMP_COMMON_SPECIFIC.md:516-803] |

### Novel Mechanisms (not in reference docs)

| Novel Element | Purpose | Why Novel |
|---------------|---------|-----------|
| Object→PlatformParent relaxation | Allow `Object→JObject` super type mismatch | Reference docs identify the problem ([REF:CJMP_COMMON_SPECIFIC.md:497-513]) but don't document the solution |
| VarDecl→PropDecl cross-kind matching | Common VarDecl matches specific PropDecl | Reference docs only cover same-kind matching ([REF:CJMP_COMMON_SPECIFIC.md:286-297]) |
| `PatchCJMPSuperCalls` | Patch `Object.init()` → `JObject.init()` in RAW CHIR | New canonicalization pass — reference docs list only 5 passes ([REF:CANONICALIZATION.md:16-18]) |
| `PatchCommonFieldsForMirror` | Replace `GetElementByName` with getter/setter Apply | Extends `UpdateMemberVarPath` ([REF:CANONICALIZATION.md:191-193]) but adds FFI-specific field→accessor replacement |
| `CHIRFieldReferencePatcher` | General CHIR field reference patching utility | No equivalent in reference docs |
| `JavaMirrorFieldHelper` | Identify mirror fields needing CHIR patching | No equivalent in reference docs |
| Getter/setter collision handling | Specific-wins + common-reuse pattern for CHIR Functions | Extends `PopulateCJMPPairs` ([REF:CJMP_PAIR_TRACKING_CHIR.md:52-61]) with Function-level collision handling |
| `ValidateCJMPConstructorConstraints` | Validate platform parent has empty constructor | Not documented in reference docs |

---

## The FROM_COMMON_PART Null Body Problem

### Problem Description

When a common-part constructor is deserialized into a specific @JavaImpl class, its
AST representation has:
- `funcBody` exists (non-null) — the constructor declaration is present
- `funcBody->body` is **null** — the body was already translated to RAW CHIR
  during common compilation

This is a direct consequence of the deferred compilation model
([REF:CJMP_COMMON_SPECIFIC.md:516-803]): common part bodies are in RAW CHIR,
not in AST. During specific compilation, the AST is reconstructed from CJO
deserialization, but function bodies that were already translated are not
re-created in AST.

### Why This Matters for the @JavaImpl Pipeline Rebase

The upstream commit `55510c72` (merged at `273e78d7`) adds new AfterTypeCheck stages
for @JavaImpl that access `funcBody->body`:

```
Crash sites at 55510c72:
  DesugarJavaImplSuperConstructorCall.cpp:308
    → CJC_NULLPTR_CHECK(superCall) — TryGetSuperCall returns null
  GenerateInJavaImplReferenceWrapper.cpp:193
    → null OwnedPtr<Block> deref (ctor->funcBody->body)
  GenerateInJavaImplReferenceWrapper.cpp:282,292
    → null Block deref (ctor.funcBody->body->body)
  GenerateNativeBridgeForJavaImp.cpp:313
    → CJC_NULLPTR_CHECK(javaSideCtor) — cache missing
```

These stages assume all @JavaImpl constructors have AST bodies. For FROM_COMMON_PART
constructors, this assumption is wrong.

### Design Options

1. **Skip FROM_COMMON_PART constructors in affected stages**:
   - Pro: Minimal code change
   - Con: FROM_COMMON_PART constructors can be the ONLY constructor in a @JavaImpl
     class (when the specific part doesn't define its own constructor). Skipping
     them entirely means no Java wrapper is generated for the class.

2. **Handle null `funcBody->body` gracefully in each stage**:
   - Pro: Each stage can make its own decision about FROM_COMMON_PART constructors
   - Con: Many stages need modification, risk of missing a null dereference

3. **Create synthetic AST body for FROM_COMMON_PART constructors**:
   - Pro: All AfterTypeCheck stages work as-is
   - Con: Synthetic body would be incorrect (common body logic ≠ @JavaImpl logic).
     Would need to be discarded anyway.

4. **CHIR-level processing for FROM_COMMON_PART constructors**:
   - Pro: Uses the actual RAW CHIR body, which is correct
   - Con: Requires new CHIR transformation passes (like `PatchCJMPSuperCalls`)

Our current implementation uses option 1 for MIRROR stages and option 4 for IMPL
stages (PatchCJMPSuperCalls). The upstream Java impl wrappers pipeline
(`GenerateInJavaImplReferenceWrapper`, `DesugarJavaImplSuperConstructorCall`,
`GenerateNativeBridgeForJavaImp`) is handled via a combination of options 1 and 2 — skip
FROM_COMMON_PART constructors where possible, handle null gracefully where
skipping isn't possible — plus CHIR-level patching for FROM_COMMON_PART constructor
bodies (PatchConstructorFieldInit, PatchFieldReferences).

---

## Deferred Compilation Stages Interaction

([REF:CJMP_COMMON_SPECIFIC.md:516-803])

Our CJMP Java FFI changes interact with the deferred compilation model in these ways:

| Deferred Stage | CJMP+FFI Interaction | Our Change |
|----------------|---------------------|------------|
| **Canonicalization** | FROM_COMMON_PART bodies need super call patching | `PatchCJMPSuperCalls` (new Pass 6) |
| **VTable Generation** | @JavaImpl/@JavaMirror classes need VTable | No change — specific VTable works for both common+specific members |
| **Field Path Resolution** (Pass 3) | Mirror field access needs getter/setter calls, not `GetElementRef` | `PatchCommonFieldsForMirror` (before Pass 3) |
| **Closure Conversion** | Common lambdas preserved in RAW CHIR | No change — closure conversion works on both common+specific bodies |
| **CodeGen** | FROM_COMMON_PART + specificImplementation duplicates must be filtered | Collision handling in `CreateFuncSignatureAndSetGlobalCache` |

The key insight: FFI transformations use symbolic names
([REF:CJMP_COMMON_SPECIFIC.md:1149-1177]):
```
Java_CFFI_getField(env, javaref, "fieldName")  — string-based, not index-based
Java_CFFI_callMethod_raw(env, javaref, "methodName") — string-based
```

This means @JavaMirror JNI calls are compatible with RAW CHIR format. They don't
need concrete field indices or VTable offsets. The FFI filtering pattern
(`FROM_COMMON_PART + specificImplementation → skip`) ensures that matched common
members don't generate duplicate JNI calls.

For @JavaImpl, the situation is different: the super call uses `Apply` with direct
Function pointer ([REF:CJMP_COMMON_SPECIFIC.md:730-759]), not symbolic reference.
This means super call patching must happen at CHIR level (after RAW CHIR is loaded)
to replace the Function pointer target.

---

## Attribute Summary

| Attribute | Purpose | Set By | Reference |
|-----------|---------|--------|-----------|
| `FROM_COMMON_PART` | Marks common-part members | CHIR deserialization (`CreateCustomTypeDef`) | [REF:CJMP_ATTRIBUTES_AND_LINKS.md:9-19] |
| `DESUGARED_MIRROR_FIELD` | Marks Var→Prop converted properties | `InsertPropConvertedByField` | [REF:JAVA_FFI_JAVAIMPL.md:37-38] |
| `COMPILER_ADD` | Protects compiler-generated fields from validation | Our changes in Insert* functions | [REF:JAVA_FFI_JAVAIMPL.md:316] |
| `IS_BROKEN` | Marks invalid CJMP members (skip in later stages) | CJMP validation guards in DesugarJavaMirror | Not in reference docs |
| `specificImplementation` | Links matched common member to specific counterpart | `LinkMatchedCommonPropToSpecific` | [REF:CJMP_ATTRIBUTES_AND_LINKS.md:36-55] |
| `COMMON` | Common declaration | Parser | [REF:CJMP_ATTRIBUTES_AND_LINKS.md:9-19] |
| `SPECIFIC` | Specific declaration | Parser | [REF:CJMP_ATTRIBUTES_AND_LINKS.md:9-19] |

---

## Diagnostic Errors Added

| Diagnostic | Condition | Pipeline Phase | FFI Sub-stage |
|------------|-----------|---------------|---------------|
| `sema_cjmp_no_empty_ctor_in_platform_parent` | Platform parent lacks empty constructor | PostTypeCheck | CJMP validation |
| `sema_cjmp_mirror_ctor_from_common_has_body` | Common constructor in @JavaMirror | PostTypeCheck | MIRROR_DESUGAR |
| `sema_cjmp_mirror_method_from_common_has_body` | Common method in @JavaMirror | PostTypeCheck | MIRROR_DESUGAR |
| `sema_cjmp_mirror_prop_from_common` | Common property in @JavaMirror | PostTypeCheck | MIRROR_DESUGAR |
| `sema_cjmp_relaxation_allowed` (note) | Object→platform parent relaxation allowed | PreTypeCheck | CJMP matching |

---

## Test Coverage

Key test cases in `cangjie_test/testsuites/LLT/compiler/CJMP/java/sema/`:

| Test | Scenario |
|------|----------|
| `java_cjmp_simple_impl_no_parent.cj` | @JavaImpl with no explicit parent (JObject auto-inserted) |
| `java_cjmp_mirror_no_parent.cj` | @JavaMirror with no explicit parent |
| `java_cjmp_mirror_full.cj` | Full @JavaMirror with common fields + specific mirror |
| `java_cjmp_impl_full.cj` | Full @JavaImpl with common methods + specific impl |
| `java_cjmp_impl_fields_common.cj` | @JavaImpl with common fields |
| `java_cjmp_impl_fields_specific.cj` | @JavaImpl with specific-only fields |
| `java_cjmp_mirror_fields_common.cj` | @JavaMirror with common fields |
| `java_cjmp_mirror_extra_methods.cj` | @JavaMirror with extra specific methods |
| `java_cjmp_impl_extra_methods.cj` | @JavaImpl with extra specific methods |
| `java_cjmp_relaxation_allowed.cj` | Object→JObject parent relaxation |
| `java_cjmp_platform_parent_no_empty_ctor.cj` | Platform parent without empty constructor (error case) |

---

## Cross-Reference Index

| Topic | This Document Section | Reference Document |
|-------|----------------------|-------------------|
| CJMP compilation model | Overview | [REF:CJMP_COMMON_SPECIFIC.md:16-25] |
| Deferred compilation stages | Deferred Compilation Stages Interaction | [REF:CJMP_COMMON_SPECIFIC.md:516-803] |
| RAW vs Canonicalized CHIR | CHIR Phase context | [REF:CJMP_COMMON_SPECIFIC.md:715-803] |
| CJMP merge pipeline | Phase 1a | [REF:CJMP_COMMON_SPECIFIC.md:59-82] |
| Object→JObject problem | Phase 1a (relaxation) | [REF:CJMP_COMMON_SPECIFIC.md:497-513] |
| FFI filtering pattern | Key Design Principles #1 | [REF:CJMP_ATTRIBUTES_AND_LINKS.md:74-86] |
| Field→Property transformation | Phase 1c | [REF:JAVA_FFI_JAVAIMPL.md:453-533] |
| Java FFI 6 sub-stages | Phase 3b | [REF:JAVA_FFI_JAVAIMPL.md:629-660] |
| Expression-based desugaring | Phase 3b (MIRROR_DESUGAR) | [REF:SEMA_FFI_OVERVIEW.md:316-347] |
| COMPILER_ADD pattern | Phase 3b (MIRROR_GENERATE_STUB) | [REF:JAVA_FFI_JAVAIMPL.md:316] |
| AST/Sema layer separation | Key Design Principles #2 | [REF:SEMA_FFI_OVERVIEW.md:167-190] |
| specificImplementation pointer | Key Design Principles #4 | [REF:CJMP_ATTRIBUTES_AND_LINKS.md:36-55] |
| CJMP pair tracking in CHIR | CHIR Phase (collision handling) | [REF:CJMP_PAIR_TRACKING_CHIR.md:52-61] |
| Canonicalization passes | CHIR Phase (canonicalization) | [REF:CANONICALIZATION.md:16-18] |
| Apply vs Invoke mechanisms | CHIR Phase context | [REF:CJMP_COMMON_SPECIFIC.md:730-759] |
| Sema three-phase pipeline | Compilation Pipeline intro | [REF:Sema.md:24-55] |
| Null body problem | The FROM_COMMON_PART Null Body Problem | [REF:CJMP_COMMON_SPECIFIC.md:516-803] |
| FFI compatibility with RAW | CHIR Phase context | [REF:CJMP_COMMON_SPECIFIC.md:1149-1177] |