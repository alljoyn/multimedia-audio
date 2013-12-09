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

package org.alljoyn.services.audio.sink;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;

/*
 * We are using a Handler because ...
 */
public class BusHandler extends Handler {
	public static String TAG = "IoEAudioStreamingPlayer";
	
	private Context mContext;

	/* These are the messages sent to the BusHandler from the UI. */
	public static final int INITIALIZE = 0;
	public static final int SHUTDOWN = 10;

	/*
	 * Native JNI methods
	 */
	static {
		System.loadLibrary("AllJoynAudioSink");
	}

	/**  */
	private native void initialize(String packageName);

	/**  */
	private native void shutdown();

	public BusHandler(Looper looper, Context context) {
		super(looper);
		this.mContext = context;
	}
	
	public void handleShutdown() {
		shutdown();
	}

	@Override
	public void handleMessage(Message msg) {
		switch (msg.what) {
		/* Connect to the bus and start our service. */
		case INITIALIZE:
		{ 
			initialize(mContext.getPackageName());
			break;
		}
		/* Release all resources acquired in connect. */
		case SHUTDOWN: {
			shutdown();
			break;   
		}
		default:
			break;
		}
	}
//
//	private void LogMessage(String msg) {
//		Log.d(TAG, msg);
//	}
}
