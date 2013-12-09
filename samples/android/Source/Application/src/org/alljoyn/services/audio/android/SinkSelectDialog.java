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

package org.alljoyn.services.audio.android;

import android.app.Activity;
import android.app.DialogFragment;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.Button;
import android.widget.ListView;

public class SinkSelectDialog extends DialogFragment {
	
	public interface SinkSelectDialogListener {
        void onDialogSinkEnable(String speakerName, String speakerPath, short speakerPort);
        void onDialogSinkDisable(String speakerName);
    }
	
	private SinkListAdapter mSinkListAdapter;
	
	public SinkSelectDialog() {
		
	}
	
	public void SetActivity(Activity activity) {
		mSinkListAdapter = new SinkListAdapter(activity, R.layout.genericitem);
	}
	
	public void AddSink(String speakerName, String speakerPath, String friendlyName, short speakerPort) {
		mSinkListAdapter.add(speakerName, speakerPath, friendlyName, speakerPort);
	}
	
	public void RemoveSink(String speakerName) {
		mSinkListAdapter.remove(speakerName);
	}
	
	static int x = 0;
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
		getDialog().getWindow().requestFeature(Window.FEATURE_NO_TITLE);
		View view = inflater.inflate(R.layout.selectspeakerdialog, container);
		ListView speakerList = (ListView) view.findViewById(R.id.speakerList);
    	speakerList.setAdapter(mSinkListAdapter);
    	Button closeButton = (Button) view.findViewById(R.id.closePopup);
    	closeButton.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View arg0) {
				getDialog().dismiss();
			}
    	});
    	
    	Button refreshButton = (Button) view.findViewById(R.id.refreshButton);
    	refreshButton.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View arg0) {
				mSinkListAdapter.clear();
				UIHelper.refreshSinks();
			}
    	});
		return view;
	}
}
