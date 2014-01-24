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

#include "MyAllJoynCode.h"
#include <unistd.h>
#include <alljoyn/audio/WavDataSource.h>
#include <alljoyn/audio/Audio.h>
#include <alljoyn/version.h>
#include <math.h>

using namespace ajn;
using namespace services;

void MyAllJoynCode::Prepare(const char* packageName) {
    QStatus status = ER_OK;

    LOGI("AllJoyn Library version: %s", ajn::GetVersion());
    LOGI("AllJoyn Library build info: %s", ajn::GetBuildInfo());
    LOGI("AllJoyn Audio version: %s", ajn::services::audio::GetVersion());
    LOGI("AllJoyn Audio build info: %s", ajn::services::audio::GetBuildInfo());

    /* Initialize AllJoyn only once */
    if (!mBusAttachment) {
        mBusAttachment = new ajn::BusAttachment(packageName, true);
        /* Start the msg bus */
        if (ER_OK == status) {
            status = mBusAttachment->Start();
        } else {
            LOGE("BusAttachment::Start failed");
        }
        /* Connect to the daemon */
        if (ER_OK == status) {
            status = mBusAttachment->Connect();
            if (ER_OK != status) {
                LOGE("BusAttachment Connect failed.");
            }
        }
        LOGD("Created BusAttachment and connected");

        /* Register the IoE AllJoyn Audio Service Service */
        mSinkPlayer = new SinkPlayer(mBusAttachment);
        mSinkPlayer->AddListener(this);
        mSinkPlayer->SetPreferredFormat(MIMETYPE_AUDIO_RAW);
        Register(mBusAttachment);
    }
}

void MyAllJoynCode::SetDataSource(const char* dataSourcePath)
{
    if (mCurrentDataSource) {
        LOGE("Must call Reset before calling SetDataSource again");
        return;
    }

    mCurrentDataSource = new WavDataSource();
    if (!((WavDataSource*)mCurrentDataSource)->Open(dataSourcePath)) {
        LOGE("Data source is not a WAV file");
        delete mCurrentDataSource;
        mCurrentDataSource = NULL;
        return;
    }
    mSinkPlayer->SetDataSource(mCurrentDataSource);
}

void MyAllJoynCode::AddSink(const char*name, const char*path, uint16_t port)
{
    if (!mSinkPlayer->HasSink(name)) {
        mSinkPlayer->AddSink(name, port, path);
    }
}

void MyAllJoynCode::RemoveSink(const char*name)
{
    if (mSinkPlayer->HasSink(name)) {
        mSinkPlayer->RemoveSink(name);
    }
}

void MyAllJoynCode::Start()
{
    mSinkPlayer->Play();
    if (wasStopped) {
        mSinkPlayer->OpenAllSinks();
    }
    wasStopped = false;
}

void MyAllJoynCode::Pause()
{
    if (mSinkPlayer->IsPlaying()) {
        mSinkPlayer->Pause();
    }
}

void MyAllJoynCode::Stop()
{
    if (mSinkPlayer->IsPlaying() && !wasStopped) {
        mSinkPlayer->CloseAllSinks();
        wasStopped = true;
    }
}

void MyAllJoynCode::Reset()
{
    mSinkPlayer->CloseAllSinks();
    wasStopped = true;

    if (mCurrentDataSource) {
        ((WavDataSource*)mCurrentDataSource)->Close();
        delete mCurrentDataSource;
        mCurrentDataSource = NULL;
    }
}

void MyAllJoynCode::ChangeVolume(float value)
{
    int16_t low;
    int16_t high;
    int16_t step;
    int16_t newVolume;
    for (std::list<qcc::String>::iterator it = mSinkNames.begin(); it != mSinkNames.end(); ++it) {
        mSinkPlayer->GetVolumeRange(it->c_str(), low, high, step);
        //value is from 0 to 1 for Android
        newVolume = ((high - low) * value) + low;
        mSinkPlayer->SetVolume(it->c_str(), newVolume);
    }
}

void MyAllJoynCode::Mute()
{
    isMuted = !isMuted;
    for (std::list<qcc::String>::iterator it = mSinkNames.begin(); it != mSinkNames.end(); ++it) {
        LOGD("Muting device: %s", it->c_str());
        mSinkPlayer->SetMute(it->c_str(), isMuted);
    }
}

void MyAllJoynCode::Release()
{
    Unregister();
    mSinkPlayer->RemoveAllSinks();
    while (mSinkPlayer->GetSinkCount() > 0)
        usleep(100 * 1000);
    delete mSinkPlayer;
    mSinkPlayer = NULL;
    /* Deallocate the BusAttachment */
    if (mBusAttachment) {
        delete mBusAttachment;
        mBusAttachment = NULL;
    }
}

void MyAllJoynCode::SinkFound(Service*sink) {
    const char*name = sink->name.c_str();
    const char*path = sink->path.c_str();
    const char*friendly = sink->friendlyName.c_str();
    LOGD("Found %s objectPath=%s, sessionPort=%d\n", name, path, sink->port);
    JNIEnv* env;
    jint jret = vm->GetEnv((void**)&env, JNI_VERSION_1_2);
    if (JNI_EDETACHED == jret) {
        vm->AttachCurrentThread(&env, NULL);
    }

    jclass jcls = env->GetObjectClass(jobj);
    jmethodID mid = env->GetMethodID(jcls, "SinkFound", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;S)V");
    if (mid == 0) {
        LOGE("Failed to get Java SinkFound");
    } else {
        jstring jName = env->NewStringUTF(name);
        jstring jPath = env->NewStringUTF(path);
        jstring jFriendly = env->NewStringUTF(friendly);
        env->CallVoidMethod(jobj, mid, jName, jPath, jFriendly, sink->port);
        env->DeleteLocalRef(jName);
        env->DeleteLocalRef(jPath);
        env->DeleteLocalRef(jFriendly);
    }
    if (JNI_EDETACHED == jret) {
        vm->DetachCurrentThread();
    }
}

void MyAllJoynCode::SinkLost(Service*sink) {
    const char*name = sink->name.c_str();
    LOGD("Lost %s\n", name);
    JNIEnv* env;
    jint jret = vm->GetEnv((void**)&env, JNI_VERSION_1_2);
    if (JNI_EDETACHED == jret) {
        vm->AttachCurrentThread(&env, NULL);
    }

    jclass jcls = env->GetObjectClass(jobj);
    jmethodID mid = env->GetMethodID(jcls, "SinkLost", "(Ljava/lang/String;)V");
    if (mid == 0) {
        LOGE("Failed to get Java SinkLost");
    } else {
        jstring jName = env->NewStringUTF(name);
        env->CallVoidMethod(jobj, mid, jName);
        env->DeleteLocalRef(jName);
    }
    if (JNI_EDETACHED == jret) {
        vm->DetachCurrentThread();
    }
}

void MyAllJoynCode::SinkAdded(const char*name) {
    mSinkNames.push_back(name);
    LOGD("SinkAdded: %s\n", name);

    mSinkPlayer->OpenSink(name);
    LOGD("SinkOpened: %s\n", name);
    JNIEnv* env;
    jint jret = vm->GetEnv((void**)&env, JNI_VERSION_1_2);
    if (JNI_EDETACHED == jret) {
        vm->AttachCurrentThread(&env, NULL);
    }

    jclass jcls = env->GetObjectClass(jobj);
    jmethodID mid = env->GetMethodID(jcls, "SinkAdded", "(Ljava/lang/String;)V");
    if (mid == 0) {
        LOGE("Failed to get Java SinkAdded");
    } else {
        jstring jName = env->NewStringUTF(name);
        env->CallVoidMethod(jobj, mid, jName);
        env->DeleteLocalRef(jName);
    }
    if (JNI_EDETACHED == jret) {
        vm->DetachCurrentThread();
    }
}

void MyAllJoynCode::SinkAddFailed(const char*name) {
    LOGD("SinkAddFailed: %s\n", name);
    JNIEnv* env;
    jint jret = vm->GetEnv((void**)&env, JNI_VERSION_1_2);
    if (JNI_EDETACHED == jret) {
        vm->AttachCurrentThread(&env, NULL);
    }

    jclass jcls = env->GetObjectClass(jobj);
    jmethodID mid = env->GetMethodID(jcls, "SinkAddFailed", "(Ljava/lang/String;)V");
    if (mid == 0) {
        LOGE("Failed to get Java SinkAddFailed");
    } else {
        jstring jName = env->NewStringUTF(name);
        env->CallVoidMethod(jobj, mid, jName);
        env->DeleteLocalRef(jName);
    }
    if (JNI_EDETACHED == jret) {
        vm->DetachCurrentThread();
    }
}

void MyAllJoynCode::SinkRemoved(const char*name, bool lost) {
    mSinkNames.remove(qcc::String(name));
    LOGD("SinkRemoved: %s lost=%d\n", name, lost);
    JNIEnv* env;
    jint jret = vm->GetEnv((void**)&env, JNI_VERSION_1_2);
    if (JNI_EDETACHED == jret) {
        vm->AttachCurrentThread(&env, NULL);
    }

    jclass jcls = env->GetObjectClass(jobj);
    jmethodID mid = env->GetMethodID(jcls, "SinkRemoved", "(Ljava/lang/String;Z)V");
    if (mid == 0) {
        LOGE("Failed to get Java SinkRemoved");
    } else {
        jstring jName = env->NewStringUTF(name);
        env->CallVoidMethod(jobj, mid, jName, lost);
        env->DeleteLocalRef(jName);
    }
    if (JNI_EDETACHED == jret) {
        vm->DetachCurrentThread();
    }
}

