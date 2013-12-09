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

public interface AllJoynAudioServiceListener {
	/**
	 * Callback that executes when a Sink has been found.
	 * 
	 * @param speakerName	Unique name of the Sink.  Pass this into other API calls.
	 * @param speakerPath	Path that the AllJoyn Audio interface is implemented.
	 * @param friendlyName	Friendly name to be used by the UI to identify a Sink.
	 * @param speakerPort	Port that is used by AllJoyn JoinSession API.
	 */
	public void SinkFound( String speakerName, String speakerPath, String friendlyName, short speakerPort );

	/**
	 * Callback that executes when a Sink is no longer available.
	 * 
	 * @param speakerName	Unique name of the Sink.
	 */
	public void SinkLost( String speakerName );

	/**
	 * Callback that executes when a Sink is ready to receive audio.
	 * 
	 * @param speakerName	Unique name of the Sink.
	 */
	public void SinkReady( String speakerName );

	/**
	 * Callback that executes when a Sink is no longer available.
	 * 
	 * @param speakerName	Unique name of the Sink.
	 * @param lost			If false implies a RemoveSink call was made, if true then Sink was lost while connected.
	 */
	public void SinkRemoved( String speakerName, boolean lost );

	/**
	 * Callback that executes when a AddSink call has failed.
	 * 
	 * @param speakerName	Unique name of the Sink.
	 */
	public void SinkError( String speakerName );
}
