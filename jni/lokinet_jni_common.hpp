#ifndef LOKINET_JNI_COMMON_HPP
#define LOKINET_JNI_COMMON_HPP

#include <jni.h>
#include <util/string_view.hpp>
#include <functional>

/// visit string as native bytes
template < typename T, typename V >
static T
VisitStringAsStringView(JNIEnv* env, jobject str, V visit)
{
  const jclass stringClass = env->GetObjectClass(str);
  const jmethodID getBytes =
      env->GetMethodID(stringClass, "getBytes", "(Ljava/lang/String;)[B");

  const jstring charsetName = env->NewStringUTF("UTF-8");
  const jbyteArray stringJbytes =
      (jbyteArray)env->CallObjectMethod(str, getBytes, charsetName);
  env->DeleteLocalRef(charsetName);

  const jsize length  = env->GetArrayLength(stringJbytes);
  const jbyte* pBytes = env->GetByteArrayElements(stringJbytes, NULL);

  T result = visit(llarp::string_view(bBytes, length));

  env->ReleaseByteArrayElements(stringJbytes, pBytes, JNI_ABORT);
  env->DeleteLocalRef(stringJbytes);

  return std::move(result);
}

/// cast jni buffer to T *
template < typename T >
static T*
FromBuffer(JNIEnv* env, jobject o)
{
  if(o == nullptr)
    return nullptr;
  return static_cast< T* >(env->GetDirectBufferAddress(o));
}

/// get T * from object member called membername
template < typename T >
static T*
FromObjectMember(JNIEnv* env, jobject self, const char* membername)
{
  jclass cl      = env->GetObjectClass(self);
  jfieldID name  = env->GetFieldID(cl, membername, "Ljava/nio/Buffer;");
  jobject buffer = env->GetObjectField(self, name);
  return FromBuffer< T >(env, buffer);
}

/// visit object string member called membername as bytes
template < typename T, typename V >
static T
VisitObjectMemberStringAsStringView(JNIEnv* env, jobject self,
                                    const char* membername, V v)
{
  jclass cl     = env->GetObjectClass(self);
  jfieldID name = env->GetFieldID(cl, membername, "Ljava/lang/String;");
  jobject str   = env->GetObjectField(self, name);
  return VisitStringAsStringView(env, str, v);
}

/// get object member int called membername
template < typename Int_t >
void
GetObjectMemberAsInt(JNIEnv* env, jobject self, const char* membername,
                     Int_t& result)
{
  jclass cl     = env->GetObjectClass(self);
  jfieldID name = env->GetFieldID(cl, membername, "I");
  result        = env->GetIntField(self, name);
}

#endif