AllJoyn Audio Samples README
------------------------------------

This directory contains the following AllJoyn Audio API samples:

SinkClient -
         This sample shows how to use AllJoyn Audio's API to discover, connect
         and stream a WAV file to AllJoyn Audio sinks.

         Example output
         $ ./SinkClient file.wav 
         AllJoyn Library version: v0.0.1
         AllJoyn Library build info: AllJoyn Library v0.0.1 (Built Tue Jul 09 20:17:35 UTC 2013 ...
         Found "Friendly Name" :OW9fG16p.2 objectPath=/Speaker/In, sessionPort=10000
         SinkAdded: :OW9fG16p.2

SinkService -   
         This sample shows how to use AllJoyn Audio's API to receive audio streams
         from AllJoyn Audio sources.

         Example output
         $ ./SinkService "Friendly Name"
         AllJoyn Library version: v0.0.1
         AllJoyn Library build info: AllJoyn Library v0.0.1 (Built Tue Jul 09 20:17:35 UTC 2013 ...
         Accepting join session request from :bnNIPZwI.2 (opts.proximity=ff, opts.traffic=1, opts.transports=4)
         SessionJoined with :bnNIPZwI.2 (id=-1313719015)
         Link timeout has been set to 40s
