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

#endif // DEFINE_MEMBER
