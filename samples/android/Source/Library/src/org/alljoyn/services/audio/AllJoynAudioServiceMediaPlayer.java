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

package org.alljoyn.services.audio;

import java.io.IOException;

import android.media.MediaPlayer;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

/**
 * A MediaPlayer that enables the ability to send audio over the AllJoyn Audio service.
 *
 * This class extends the MediaPlayer to add the ability to communicate with AllJoyn
 * Audio service Sinks.  It is both a re-usable library and an example application.
 * The jni wrapper code is an implementation of using the AllJoyn Audio service APIs,
 * see the AllJoyn Audio Service Usage Guide for more details.
 *
 * The "Getting Started with the AllJoyn Audio Service using Java" details how you can
 * setup your environment in Android to use this library.
 */	
public class AllJoynAudioServiceMediaPlayer extends MediaPlayer {
	private static final String TAG = "AllJoynAudioServiceMediaPlayer";
	
	/* Handler used to make calls to AllJoyn methods. See onCreate(). */
    private Handler mBusHandler;
    private AllJoynAudioServiceListener mListener;
    private boolean mStreamOverAllJoyn = false;
    private boolean isPlayingOverStream = false;
    private int numEnabledSinks = 0;
    private String mDataSourcePath;
    
    /* These are the messages sent to the BusHandler to avoid blocking the UI. */
    private static final int PREPARE = 0;
    private static final int SET_DATA_SOURCE = 1;
    private static final int SET_ALLJOYN_SPEAKER = 2;
    private static final int REMOVE_ALLJOYN_SPEAKER = 3;
	private static final int START_STREAMING = 4;
	private static final int PAUSE_STREAMING = 5;
	private static final int STOP_STREAMING = 6;
	private static final int CHANGE_VOLUME = 7;
	private static final int MUTE_VOLUME = 8;
	private static final int REFRESH_SINKS = 9;
	private static final int RESET = 10;
	private static final int RELEASE = -1;

	/*
	 * Native JNI methods
	 */
	static {
		System.loadLibrary("easy_alljoyn_audio_service");
	}

	/** Native jni call to startup and initialize the AllJoyn Audio service */
	private native void Prepare(String packageName);

	/** Native jni call to set the source file to be used */
	private native void SetDataSource(String dataSource);
	
	/** Native jni call to add a Sink to receive audio */
	private native void AddSink(String name, String path, short port);
	
	/** Native jni call to remove a Sink from receiving audio */
	private native void RemoveSink(String name);
	
	/** Native jni call to start sending audio.  Must call SetDataSource and AddSink prior to this method. */
	private native void Start();
	
	/** Native jni call to pause sending audio. */
	private native void Pause();
	
	/** Native jni call to stop sending audio. */
	private native void Stop();

	/** Native jni call to stop sending audio and reset variables. */
	private native void Reset();
	
	/** Native jni call to change volume on all added Sinks. */
	private native void ChangeVolume(float volume);
	
	/** Native jni call to mute the volume all added Sinks. */
	private native void Mute();

	/** Native jni call to release and shutdown. */
	private native void Release();
	
	/** Native jni call to trigger a refresh of Sink information. */
	private native void RefreshSinks();
    
	/**
	 * Constructor to create an AllJoynAudioServie Media Player
	 */
    public AllJoynAudioServiceMediaPlayer() {
    	/* Make all AllJoyn calls through a separate handler thread to prevent blocking the UI. */
        HandlerThread busThread = new HandlerThread("BusHandler");
        busThread.start();

    	mBusHandler = new BusHandler(busThread.getLooper());
        mBusHandler.sendEmptyMessage(PREPARE);
        numEnabledSinks = 0;
    }
    
    /**
     * Set the listener to receive callback information from the Audio Service.
     * 
     * @param listener	The AllJoyn Audio Service Listener that will be triggered when events occur in the AllJoyn Audio service.
     */
    public void setAllJoynAudioServiceListener(AllJoynAudioServiceListener listener) {
    	mListener = listener;
    }
     
    
    /**
     * Just like the Android MediaPlayer this sets the source file that will be used to play audio.
     * 
     * @param path	Path to the audio file to be used.  For AllJoyn audio we currently support .wav files.
     */
    @Override
    public void setDataSource(String path) throws IllegalStateException, IOException, IllegalArgumentException, SecurityException {
    	//Should stream over alljoyn
    	//Check if the path is for a wav file, if so pass it on down and let it stream as is
    	mDataSourcePath = path;
    	if(path.endsWith(".wav")) {
    		mBusHandler.sendMessage(mBusHandler.obtainMessage(SET_DATA_SOURCE, path));
    	}
    	try{
    		super.setDataSource(path);
    	}catch(IllegalStateException e) {
    		throw e;
    	}catch(IOException e) {
    		throw e;
    	}catch(IllegalArgumentException e) {
    		throw e;
    	}catch(SecurityException e) {
    		throw e;
    	}
    }
    
    /**
     * Add a Sink that will receive audio for playback.
     * Values are provided by the AllJoyn Audio Service Listener callbacks.
     * 
     * @param speakerName	The name value returned from a AllJoyn Audio Service Listener SinkFound callback.
     * @param speakerPath	The path value returned from a AllJoyn Audio Service Listener SinkFound callback.
     * @param speakerPort	The port value returned from a AllJoyn Audio Service Listener SinkFound callback.
     */
    public void addAllJoynSink(String speakerName, String speakerPath, short speakerPort) {
    	if(mDataSourcePath == null || mDataSourcePath.length() == 0) {
    		throw new IllegalStateException("Call setDataSource prior to calling add");
    	}
    	mStreamOverAllJoyn = true;
    	numEnabledSinks++;
    	Message msg = mBusHandler.obtainMessage(SET_ALLJOYN_SPEAKER);
    	Bundle data = new Bundle();
    	data.putString("name", speakerName);
    	data.putString("path", speakerPath);
    	data.putShort("port", speakerPort);
    	msg.setData(data);
    	mBusHandler.sendMessage(msg);
    }
    
    /**
     * Remove a Sink from playing audio.
     * 
     * @param speakerName	The same name used when adding a Sink.
     */
    public void removeAllJoynSink(String speakerName) {
    	numEnabledSinks--;
    	if(numEnabledSinks == 0)
    		mStreamOverAllJoyn = false;
    	Message msg = mBusHandler.obtainMessage(REMOVE_ALLJOYN_SPEAKER);
    	Bundle data = new Bundle();
    	data.putString("name", speakerName);
    	msg.setData(data);
    	mBusHandler.sendMessage(msg);
    }
    
    /**
     * Informs the AllJoyn Audio service to refresh its list of Sinks.
     * This will cause the AllJoyn Audio Service Listener SinkFound callback to trigger again.
     */
    public void refreshSinks() {
    	mBusHandler.sendEmptyMessage(REFRESH_SINKS);
    }
    
    /**
     * Extension of the MediaPlayer start.  Will cause audio to start sending if a Sink has been added.
     * If no Sink added, functions the same as the Android MediaPlayer.
     */
    @Override
    public void start() {
    	if(!mStreamOverAllJoyn) {
    		super.start();
    		return;
    	}
    	isPlayingOverStream = true;
    	mBusHandler.sendEmptyMessage(START_STREAMING);
    	if(super.isPlaying())
    		super.stop();
    }
    
    /**
     * Extension of the MediaPlayer pause.  Will cause audio to pause if a Sink has been added.
     * If no Sink added, functions the same as the Android MediaPlayer.
     */
    @Override
    public void pause() {
    	if(!mStreamOverAllJoyn) {
    		super.pause();
    		return;
    	}
    	isPlayingOverStream = false;
    	mBusHandler.sendEmptyMessage(PAUSE_STREAMING);
    }
    
    /**
     * Extension of the MediaPlayer stop.  Will cause audio to stop sending if a Sink has been added.
     * If no Sink added, functions the same as the Android MediaPlayer.
     */
    @Override
    public void stop() {
    	if(!mStreamOverAllJoyn) {
    		super.stop();
    		return;
    	}
    	isPlayingOverStream = false;
    	mBusHandler.sendEmptyMessage(STOP_STREAMING);
    }
    
    /**
     * Extension of the MediaPlayer reset.  Will cause audio to stop sending if a Sink has been added and resets states.
     * If no Sink added, functions the same as the Android MediaPlayer.
     */
    @Override
    public void reset() {
    	super.reset();
    	isPlayingOverStream = false;
    	mBusHandler.sendEmptyMessage(RESET);
    }
    
    /**
     * Extension of the MediaPlayer isPlaying.
     * 
     * @return Android MediaPlayer.isPlaying() if not streaming audio else if audio is in a Play state on Sinks.
     */
    @Override
    public boolean isPlaying() {
    	if(!mStreamOverAllJoyn)
    		return super.isPlaying();
    	return isPlayingOverStream;
    }
    
    /**
     * Extension of the MediaPlayer setVolume.  Will change volume on all Sinks if any have been added.
     * If no Sink added, functions the same as the Android MediaPlayer.
     * 
     * Note: If a Sink is added only the leftVolume is used to control volume levels on Sinks.
     * 		Setting leftVolume to 0 will mute all added Sinks.
     * 
     * @param leftVolume Volume to set the left channel.
     * @param rightVolume Volume to set the right channel.
     */
    @Override
    public void setVolume(float leftVolume, float rightVolume) {
    	if(!mStreamOverAllJoyn) {
    		super.setVolume(leftVolume, rightVolume);
    		return;
    	}
    	Message msg = mBusHandler.obtainMessage(leftVolume == 0 ? MUTE_VOLUME : CHANGE_VOLUME);
    	Bundle data = new Bundle();
    	data.putFloat("volume", leftVolume);
    	msg.setData(data);
    	mBusHandler.sendMessage(msg);
    }
    
    /**
     * Extension of the MediaPlayer release call.  Cleans up the MediaPlayer and the AllJoyn Audio Service
     */
    @Override
    public void release() {
    	super.release();
    	mBusHandler.sendEmptyMessage(RELEASE);
    }
    
    
    /**
     * Private class to avoid locking up the UI thread when making calles into the AllJoyn Audio Service.
     */
    private class BusHandler extends Handler {
    	public BusHandler(Looper looper) {
    		super(looper);
    	}

    	@Override
    	public void handleMessage(Message msg) {
    		Bundle data = msg.getData();
    		switch (msg.what) {
    		/* Connect to the bus and start our service. */
    		case PREPARE:
    			Prepare("alljoyn.audio.streaming.IoE");
    			break;
    		case SET_DATA_SOURCE:
    			SetDataSource((String)msg.obj);
    			break;
    		case SET_ALLJOYN_SPEAKER:
    			AddSink(data.getString("name"), data.getString("path"), data.getShort("port"));
    			break;
    		case REMOVE_ALLJOYN_SPEAKER:
    			RemoveSink(data.getString("name"));
    			break;
    		case START_STREAMING:
    			Start();
    			break;
    		case PAUSE_STREAMING:
    			Pause();
    			break;
    		case STOP_STREAMING:
    			Stop();
    			break;
    		case RESET:
    			Reset();
    			break;
    		case CHANGE_VOLUME:
    			ChangeVolume(data.getFloat("volume"));
    			break;
    		case MUTE_VOLUME:
    			Mute();
    			break;
    		/* Release all resources acquired in connect. */
    		case RELEASE:
    			Release();
    			break;
    		case REFRESH_SINKS:
    			RefreshSinks();
    			break;
    		default:
    			Log.d(TAG,"DEFAULT CASE STATEMENT IN BUSHANDLER");
    			break;
    		}
    	}
    }
    
    /**
     * Private method that is invoked when the jni C++ SinkSearcher class callback for SinkFound occurs.
     */
	private void SinkFound( String name, String path, String friendlyName, short port ) {
		if(mListener != null)
			mListener.SinkFound(name, path, friendlyName, port);
	}

	/**
     * Private method that is invoked when the jni C++ SinkSearcher class callback for SinkLost occurs.
     */
	private void SinkLost( String name ) {
		if(mListener != null)
			mListener.SinkLost(name);
	}

	/**
     * Private method that is invoked when the jni C++ SinkListener class callback for SinkAdded occurs.
     */
	private void SinkAdded( String name ) {
		if(mListener != null)
			mListener.SinkReady(name);
	}

	/**
     * Private method that is invoked when the jni C++ SinkListener class callback for SinkAddFailed occurs.
     */
	private void SinkAddFailed( String name ) {
		if(mListener != null)
			mListener.SinkError(name);
	}

	/**
     * Private method that is invoked when the jni C++ SinkListener class callback for SinkRemoved occurs.
     */
	private void SinkRemoved( String name, boolean lost ) {
		if(mListener != null)
			mListener.SinkRemoved(name, lost);
	}
}
