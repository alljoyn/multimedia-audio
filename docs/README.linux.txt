
AllJoyn Audio SDK for Linux
---------------------

This subtree contains one complete copy of the AllJoyn Audio SDK for Linux,
built for a single CPU type (either x86 or x86_64) and VARIANT (either debug or
release).  The CPU and VARIANT are normally incorporated into the name of the
package or folder containing this SDK.

Please see ReleaseNotes.txt for the applicable AllJoyn Audio release version and
related information on new features and known issues.


Summary of file and directory structure:
----------------------------------------

The contents of this SDK are arranged into the following top level folders:

cpp/    core AllJoyn Audio functionality, implemented in C++


The contents of each top level folder are further arranged into sub-folders:

        ----------------------------------------------------
cpp/    core AllJoyn Audio functionality, implemented in C++
        ----------------------------------------------------

    bin/samples/                pre-built sample programs

    docs/html/                  AllJoyn Audio API documentation

    inc/alljoyn/audio           AllJoyn Audio (C++) headers

    lib/                        AllJoyn Audio (C++) client libraries

        liballjoyn_audio.a      implements audio API

    samples/                    C++ sample programs (see README)
