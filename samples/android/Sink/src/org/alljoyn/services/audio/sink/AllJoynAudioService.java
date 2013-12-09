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

import android.app.Service;
import android.content.Intent;
import android.os.HandlerThread;
import android.os.IBinder;

public class AllJoynAudioService extends Service{
	private BusHandler mBusHandler;
	
	@Override
	public IBinder onBind(Intent arg0) {
		// TODO Auto-generated method stub
		return null;
	}
	
	@Override
	public void onCreate() {
		super.onCreate();
		setupService();
	}
		
	private void setupService() {	
		if(mBusHandler != null)
			return;
		/* Make all AllJoyn calls through a separate handler thread to prevent blocking the UI. */
        HandlerThread busThread = new HandlerThread("BusHandler");
        busThread.start();
    	mBusHandler = new BusHandler(busThread.getLooper(), this);
        mBusHandler.sendEmptyMessage(BusHandler.INITIALIZE);
	}
	 
	@Override
	public void onDestroy() {
		mBusHandler.handleShutdown();
	}
	
	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		setupService();
		return START_STICKY;
	}
}
