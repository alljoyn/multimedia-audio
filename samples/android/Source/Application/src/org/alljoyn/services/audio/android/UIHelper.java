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

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.alljoyn.services.audio.*;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.FragmentManager;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.AdapterView.OnItemClickListener;

public class UIHelper {
    
	private Activity mActivity;
    private static UIHelper instance;
    
    private AllJoynAudioServiceMediaPlayer mMediaPlayer;
    private boolean mWasPaused = false;
    
    private SinkSelectDialog mSinkSelectDialog;
    private List<String> mSongFileList = new ArrayList<String>();
    private Button mPlayButton;
    private Button mPauseButton;
    private Button mStopButton;
    private Button mSelectSinkButton;
    private Button mMuteSinksButton;
    private float volume = 1.0F;
    private View mPrevSelectedSong = null;
    private String mSelectedDataSource = null;
    
    public UIHelper(Activity activity) {
    	instance = this;
    	this.mActivity = activity;
    	mMediaPlayer = new AllJoynAudioServiceMediaPlayer();
    	mSinkSelectDialog = new SinkSelectDialog();
    	mSinkSelectDialog.SetActivity(activity);
    }
    
    public void init() {
    	mMediaPlayer.setAllJoynAudioServiceListener(new AllJoynAudioServiceListener() {
			@Override
			public void SinkFound(final String speakerName, final String speakerPath, final String friendlyName, final short speakerPort) {
				mActivity.runOnUiThread(new Runnable() {
					public void run() {
						mSinkSelectDialog.AddSink(speakerName, speakerPath, friendlyName, speakerPort);
					}
				});
			}

			@Override
			public void SinkLost(final String speakerName) {
				mActivity.runOnUiThread(new Runnable() {
					public void run() {
						mSinkSelectDialog.RemoveSink(speakerName);
					}
				});
			}

			@Override
			public void SinkReady(String speakerName) {
				//Sink that we connected to is now ready lets call play
				boolean isPlaying = mMediaPlayer.isPlaying();
				if(!isPlaying)
					setupSong();
				playSong();
			}
			
			@Override
			public void SinkRemoved(final String speakerName, boolean lost) {
				if(lost) {
					mActivity.runOnUiThread(new Runnable() {
						public void run() {
							mSinkSelectDialog.RemoveSink(speakerName);
						}
					});				
				}
			}

			@Override
			public void SinkError(String speakerName) {
				//Publish error message
			}
    	});
    	
    	mMediaPlayer.setVolume(volume, volume);
    	
    	ListView songListView = (ListView) mActivity.findViewById(R.id.songList);
    	songListView.setAdapter(new ArrayAdapter<String>(mActivity, R.layout.songitem, buildSongList()));
    	songListView.setOnItemClickListener(new OnItemClickListener() {
			@Override
			public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
				if(mPrevSelectedSong != null)
					mPrevSelectedSong.setBackgroundColor(Color.TRANSPARENT);
				view.setSelected(true);
				view.setBackgroundColor(Color.parseColor("#99555555"));
				mPrevSelectedSong = view;
				mSelectedDataSource = mSongFileList.get(position);
				boolean isPlaying = mMediaPlayer.isPlaying();
				setupSong();
				if(isPlaying)
					playSong();
			}
    	});
    	
    	mPlayButton = (Button)mActivity.findViewById(R.id.playButton);
    	mPlayButton.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View arg0) {
				if(!mWasPaused)
					setupSong();
				playSong();
			}
    	});
    	
    	mPauseButton = (Button)mActivity.findViewById(R.id.pauseButton);
    	mPauseButton.setOnClickListener(new OnClickListener() {
    		@Override
			public void onClick(View arg0) {
    			try{
    				mMediaPlayer.pause();
    				mWasPaused = true;
    			}catch(Exception e) {
    				e.printStackTrace();
    			}
			}
    	});
    	
    	mStopButton = (Button)mActivity.findViewById(R.id.stopButton);
    	mStopButton.setOnClickListener(new OnClickListener() {
    		@Override
			public void onClick(View arg0) {
    			try{
					mMediaPlayer.stop();
					mWasPaused = false;
    			}catch(Exception e) {
    				e.printStackTrace();
    			}
			}
    	});
    	
    	mSelectSinkButton = (Button)mActivity.findViewById(R.id.pickSink);
    	mSelectSinkButton.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View arg0) {
				showSinkSelectDiaglog();
			}
    	});
    	
    	mMuteSinksButton = (Button)mActivity.findViewById(R.id.muteAll);
    	mMuteSinksButton.setOnClickListener(new OnClickListener() {
    		@Override
    		public void onClick(View arg0) {
    			mMediaPlayer.setVolume(0, 0);
    			if(mMuteSinksButton.getText().toString().startsWith("Mute"))
    				mMuteSinksButton.setText("Unmute All Sinks");
    			else
    				mMuteSinksButton.setText("Mute All Sinks");
    		}
    	});
    }
    
    private void setupSong() {
    	try {
    		mWasPaused = false;
			mMediaPlayer.reset();
			mMediaPlayer.setDataSource(mSelectedDataSource);
			mMediaPlayer.prepare();
		} catch (IllegalStateException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
    }
    
    private void playSong() {
    	try {
    		mMediaPlayer.seekTo(0);
			mMediaPlayer.start();
		} catch (IllegalStateException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		}
    }
    
    public void onDestroy() {
    	mMediaPlayer.release();
    }
    
    public boolean onKeyDown(int keyCode, KeyEvent event) {
    	switch (keyCode) {
        case KeyEvent.KEYCODE_VOLUME_UP:
        	volume += 0.1F;
        	if(volume > 1)
        		volume = 1;
        	mMediaPlayer.setVolume(volume, volume);
            return true;
        case KeyEvent.KEYCODE_VOLUME_DOWN:
        	volume -= 0.1F;
        	if(volume < 0)
        		volume = 0;
        	mMediaPlayer.setVolume(volume, volume);
            return true;
    	}
    	return false;
    }
   
    private void showSinkSelectDiaglog() {
    	FragmentManager fragmentManager = mActivity.getFragmentManager();
    	mSinkSelectDialog.show(fragmentManager, "SinkSelectDialog");
    }
    
    public void onDialogSinkEnable(String speakerName, String speakerPath, short speakerPort) {
    	boolean isPlaying = mMediaPlayer.isPlaying();
		mMediaPlayer.addAllJoynSink(speakerName, speakerPath, speakerPort);
		if(isPlaying)
			mMediaPlayer.start();
    }
    
    public void onDialogSinkDisable(String speakerName) {
		mMediaPlayer.removeAllJoynSink(speakerName);
    }
    
    private String[] buildSongList() {
    	mActivity.sendBroadcast(new Intent(Intent.ACTION_MEDIA_MOUNTED,
    			Uri.parse("file://"+ Environment.getExternalStorageDirectory()))); 
    	
    	String[] fields = { MediaStore.Audio.Media._ID,
                MediaStore.Audio.Media.DATA,
                MediaStore.Audio.Media.TITLE,
                MediaStore.Audio.Artists.ARTIST,
                MediaStore.Audio.Albums.ALBUM,
                MediaStore.Audio.Albums.ALBUM_ID,
                MediaStore.Audio.Media.DISPLAY_NAME};
        
        Cursor cursor = mActivity.managedQuery(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, fields, null, null, null);
        
        List<String> songList = new ArrayList<String>();
        // Clear the file list to replace its contents with current file list.
        mSongFileList.clear();

        while(cursor != null && cursor.moveToNext() && songList != null) {
            if (!cursor.getString(1).endsWith(".wav")) {
                // the media streamer will only work for wav files skip any other file type
                continue;
            }
            songList.add(cursor.getString(2) + "\nby " + cursor.getString(3));
            mSongFileList.add(cursor.getString(1));
        }
        
        if(songList.size() == 0) {
        	AlertDialog.Builder alertBuilder = new AlertDialog.Builder(mActivity);
    		alertBuilder.setMessage("Please install wave files on device and then relaunch the application.");
    		alertBuilder.setNegativeButton("Quit", new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface arg0, int arg1) {
					mActivity.finish();
			    	android.os.Process.killProcess(android.os.Process.myPid());
				}
    		});
    		alertBuilder.setCancelable(false);
    		alertBuilder.create().show();
        }
        
        return songList.toArray(new String[songList.size()]);
    }
    
    public static void refreshSinks() {
    	instance.mMediaPlayer.refreshSinks();
    }
}
