/******************************************************************************
 * Copyright (c) 2013, AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include <jni.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include "Constants.h"
#include "MyAllJoynCode.h"

/* Static data */
static MyAllJoynCode* myAllJoynCode = NULL;

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     Java_org_alljoyn_services_audio_sink_BusHandler_initialize
 * Method:    initialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_sink_BusHandler_initialize(JNIEnv* env, jobject jobj, jstring packageNameStrObj) {
    if (myAllJoynCode == NULL) {
        JavaVM* vm;
        env->GetJavaVM(&vm);
        jobject gjobj = env->NewGlobalRef(jobj);
        myAllJoynCode = new MyAllJoynCode();
    }
    jboolean iscopy;
    const char* packageNameStr = env->GetStringUTFChars(packageNameStrObj, &iscopy);
    myAllJoynCode->initialize(packageNameStr);
    env->ReleaseStringUTFChars(packageNameStrObj, packageNameStr);
}

/*
 * Class:     Java_org_alljoyn_services_audio_sink_BusHandler_shutdown
 * Method:    shutdown
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_sink_BusHandler_shutdown(JNIEnv* env, jobject jobj) {
    myAllJoynCode->shutdown();
    delete myAllJoynCode;
    myAllJoynCode = NULL;
}

#ifdef __cplusplus
}
#endif

