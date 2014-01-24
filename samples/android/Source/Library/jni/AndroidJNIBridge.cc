/******************************************************************************
 * Copyright (c) 2013-2014, AllSeen Alliance. All rights reserved.
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
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    Prepare
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_Prepare(JNIEnv* env, jobject jobj, jstring packageNameStrObj) {
    if (myAllJoynCode == NULL) {
        JavaVM* vm;
        env->GetJavaVM(&vm);
        jobject gjobj = env->NewGlobalRef(jobj);
        myAllJoynCode = new MyAllJoynCode(vm, gjobj);
    }
    jboolean iscopy;
    const char* packageNameStr = env->GetStringUTFChars(packageNameStrObj, &iscopy);
    myAllJoynCode->Prepare(packageNameStr);
    env->ReleaseStringUTFChars(packageNameStrObj, packageNameStr);
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    SetDataSource
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_SetDataSource(JNIEnv* env, jobject jobj, jstring jPath) {
    jboolean iscopy;
    const char* path = env->GetStringUTFChars(jPath, &iscopy);
    if (myAllJoynCode != NULL) {
        myAllJoynCode->SetDataSource(path);
    }
    env->ReleaseStringUTFChars(jPath, path);
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    AddSink
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_AddSink(JNIEnv* env, jobject jobj, jstring jName, jstring jPath, jshort port) {
    jboolean iscopy;
    const char* name = env->GetStringUTFChars(jName, &iscopy);
    const char* path = env->GetStringUTFChars(jPath, &iscopy);
    if (myAllJoynCode != NULL) {
        myAllJoynCode->AddSink(name, path, port);
    }
    env->ReleaseStringUTFChars(jName, name);
    env->ReleaseStringUTFChars(jPath, path);
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    RemoveSink
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_RemoveSink(JNIEnv* env, jobject jobj, jstring jName) {
    jboolean iscopy;
    const char* name = env->GetStringUTFChars(jName, &iscopy);
    if (myAllJoynCode != NULL) {
        myAllJoynCode->RemoveSink(name);
    }
    env->ReleaseStringUTFChars(jName, name);
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    Start
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_Start(JNIEnv* env, jobject jobj) {
    if (myAllJoynCode != NULL) {
        myAllJoynCode->Start();
    }
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    Pause
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_Pause(JNIEnv* env, jobject jobj) {
    if (myAllJoynCode != NULL) {
        myAllJoynCode->Pause();
    }
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    Stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_Stop(JNIEnv* env, jobject jobj) {
    if (myAllJoynCode != NULL) {
        myAllJoynCode->Stop();
    }
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    Reset
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_Reset(JNIEnv* env, jobject jobj) {
    if (myAllJoynCode != NULL) {
        myAllJoynCode->Reset();
    }
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    ChangeVolume
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_ChangeVolume(JNIEnv* env, jobject jobj, jfloat value) {
    if (myAllJoynCode != NULL) {
        myAllJoynCode->ChangeVolume(value);
    }
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    Mute
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_Mute(JNIEnv* env, jobject jobj) {
    if (myAllJoynCode != NULL) {
        myAllJoynCode->Mute();
    }
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    Release
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_Release(JNIEnv* env, jobject jobj) {
    myAllJoynCode->Release();
    delete myAllJoynCode;
    myAllJoynCode = NULL;
}

/*
 * Class:     org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer
 * Method:    Release
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_alljoyn_services_audio_AllJoynAudioServiceMediaPlayer_RefreshSinks(JNIEnv* env, jobject jobj) {
    if (myAllJoynCode != NULL) {
        myAllJoynCode->Refresh();
    }
}

#ifdef __cplusplus
}
#endif

