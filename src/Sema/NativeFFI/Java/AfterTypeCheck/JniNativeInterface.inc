// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.
// ASTKIND(ast kind, ast value for human reading, ast node, ast size for notify checking related code)
#ifdef DEFINE_MEMBER

// jobject NewGlobalRef(JNIEnv *env, jobject obj);
DEFINE_MEMBER(NewGlobalRef, VAR_DECL, 1)

// jweak NewWeakGlobalRef(JNIEnv *env, jobject obj);
DEFINE_MEMBER(NewWeakGlobalRef, VAR_DECL, 1)

// jobject NewObject(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(NewObject, VAR_DECL, 2)
// jobject NewObjectA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(NewObjectA, VAR_DECL, 3)

// jfieldID GetFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig);
DEFINE_MEMBER(GetFieldID, VAR_DECL, 3)
// jfieldID GetStaticFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig);
DEFINE_MEMBER(GetStaticFieldID, VAR_DECL, 3)

// jmethodID GetMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig);
DEFINE_MEMBER(GetMethodID, VAR_DECL, 3)
// jmethodID GetStaticMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig);
DEFINE_MEMBER(GetStaticMethodID, VAR_DECL, 3)

// jbyte CallByteMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallByteMethod, VAR_DECL, 2)
// jbyte CallByteMethodA (JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallByteMethodA, VAR_DECL, 3)

// jbyte CallNonvirtualByteMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualByteMethod, VAR_DECL, 3)
// jbyte CallNonvirtualByteMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualByteMethodA, VAR_DECL, 4)

// jbyte CallStaticByteMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticByteMethod, VAR_DECL, 2)
// jbyte CallStaticByteMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticByteMethodA, VAR_DECL, 3)

// jchar CallCharMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallCharMethod, VAR_DECL, 2)
// jchar CallCharMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallCharMethodA, VAR_DECL, 3)

// jchar CallNonvirtualCharMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualCharMethod, VAR_DECL, 3)
// jchar CallNonvirtualCharMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualCharMethodA, VAR_DECL, 4)

// jchar CallStaticCharMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticCharMethod, VAR_DECL, 2)
// jchar CallStaticCharMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticCharMethodA, VAR_DECL, 3)


// jshort CallShortMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallShortMethod, VAR_DECL, 2)
// jshort CallShortMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallShortMethodA, VAR_DECL, 3)

// jshort CallNonvirtualShortMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualShortMethod, VAR_DECL, 3)
// jshort CallNonvirtualShortMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualShortMethodA, VAR_DECL, 4)

// jshort CallStaticShortMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticShortMethod, VAR_DECL, 2)
// jshort CallStaticShortMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticShortMethodA, VAR_DECL, 3)


// jint CallIntMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallIntMethod, VAR_DECL, 2)
// jint CallIntMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallIntMethodA, VAR_DECL, 3)

// jint CallNonvirtualIntMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualIntMethod, VAR_DECL, 3)
// jint CallNonvirtualIntMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualIntMethodA, VAR_DECL, 4)

// jint CallStaticIntMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticIntMethod, VAR_DECL, 2)
// jint CallStaticIntMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticIntMethodA, VAR_DECL, 3)


// jlong CallLongMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallLongMethod, VAR_DECL, 2)
// jlong CallLongMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallLongMethodA, VAR_DECL, 3)

// jlong CallNonvirtualLongMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualLongMethod, VAR_DECL, 3)
// jlong CallNonvirtualLongMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualLongMethodA, VAR_DECL, 4)

// jlong CallStaticLongMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticLongMethod, VAR_DECL, 2)
// jlong CallStaticLongMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticLongMethodA, VAR_DECL, 3)


// jfloat CallFloatMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallFloatMethod, VAR_DECL, 2)
// jfloat CallFloatMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallFloatMethodA, VAR_DECL, 3)

// jfloat CallNonvirtualFloatMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualFloatMethod, VAR_DECL, 3)
// jfloat CallNonvirtualFloatMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualFloatMethodA, VAR_DECL, 4)

// jfloat CallStaticFloatMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticFloatMethod, VAR_DECL, 2)
// jfloat CallStaticFloatMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticFloatMethodA, VAR_DECL, 3)


// jdouble CallDoubleMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallDoubleMethod, VAR_DECL, 2)
// jdouble CallDoubleMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallDoubleMethodA, VAR_DECL, 3)

// jdouble CallNonvirtualDoubleMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualDoubleMethod, VAR_DECL, 3)
// jdouble CallNonvirtualDoubleMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualDoubleMethodA, VAR_DECL, 4)

// jdouble CallStaticDoubleMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticDoubleMethod, VAR_DECL, 2)
// jdouble CallStaticDoubleMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticDoubleMethodA, VAR_DECL, 3)


// jboolean CallBooleanMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallBooleanMethod, VAR_DECL, 2)
// jboolean CallBooleanMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallBooleanMethodA, VAR_DECL, 3)

// jboolean CallNonvirtualBooleanMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualBooleanMethod, VAR_DECL, 3)
// jboolean CallNonvirtualBooleanMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualBooleanMethodA, VAR_DECL, 4)

// jboolean CallStaticBooleanMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticBooleanMethod, VAR_DECL, 2)
// jboolean CallStaticBooleanMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticBooleanMethodA, VAR_DECL, 3)


// void CallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallVoidMethod, VAR_DECL, 2)
// void CallVoidMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallVoidMethodA, VAR_DECL, 3)

// void CallNonvirtualVoidMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualVoidMethod, VAR_DECL, 3)
// void CallNonvirtualVoidMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualVoidMethodA, VAR_DECL, 4)

// void CallStaticVoidMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticVoidMethod, VAR_DECL, 2)
// void CallStaticVoidMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticVoidMethodA, VAR_DECL, 3)


// jobject CallObjectMethod(JNIEnv *env, jobject obj, jmethodID methodID);
DEFINE_MEMBER(CallObjectMethod, VAR_DECL, 2)
// jobject CallObjectMethodA(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallObjectMethodA, VAR_DECL, 3)

// jobject CallNonvirtualObjectMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallNonvirtualObjectMethod, VAR_DECL, 3)
// jobject CallNonvirtualObjectMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallNonvirtualObjectMethodA, VAR_DECL, 4)

// jobject CallStaticObjectMethod(JNIEnv *env, jclass clazz, jmethodID methodID);
DEFINE_MEMBER(CallStaticObjectMethod, VAR_DECL, 2)
// jobject CallStaticObjectMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args);
DEFINE_MEMBER(CallStaticObjectMethodA, VAR_DECL, 3)

#endif // DEFINE_MEMBER
