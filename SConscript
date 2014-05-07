# Copyright (c) 2013-2014, AllSeen Alliance. All rights reserved.
#
#    Permission to use, copy, modify, and/or distribute this software for any
#    purpose with or without fee is hereby granted, provided that the above
#    copyright notice and this permission notice appear in all copies.
#
#    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import os

Import('env')

env['_ALLJOYN_AUDIO_'] = True

# Make Audio service library and header file paths available to the global environment
env.Append(CPPPATH = '$DISTDIR/audio/inc')
env.Append(LIBPATH = '$DISTDIR/audio/lib')

if not env.has_key('_ALLJOYN_ABOUT_') and os.path.exists('../about/SConscript'):
    env.SConscript('../about/SConscript')

if 'cpp' in env['bindings'] and not env.has_key('_ALLJOYNCORE_') and os.path.exists('../../alljoyn_core/SConscript'):
    env.SConscript('../../alljoyn_core/SConscript')


# Don't pollute the global build environment with Audio specific options.
audio_env = env.Clone()

if not audio_env.has_key('_ALLJOYNCORE_'):
    audio_env.Append(LIBS = [ 'alljoyn' ])
    if audio_env['OS'] == 'linux':
        audio_env.AppendUnique(LIBS = [ 'asound' ])
    if audio_env['OS'] == 'openwrt':
        audio_env.AppendUnique(LIBS = [ 'stdc++', 'pthread' ])

vars = Variables()
vars.Add(PathVariable('ALAC_BASE', 'Base directory of ALAC', os.environ.get('ALAC_BASE')))
vars.Update(audio_env)

Help(vars.GenerateHelpText(audio_env))

# Make audio dist a sub-directory of the alljoyn dist.  This avoids any conflicts with alljoyn dist targets.
audio_env['AUDIO_DISTDIR'] = audio_env['DISTDIR'] + '/audio'
audio_env['AUDIO_TESTDIR'] = audio_env['TESTDIR'] + '/audio'



# Add support for multiple build targets in the same workset
audio_env.VariantDir('$OBJDIR', 'src', duplicate = 0)
audio_env.VariantDir('$OBJDIR/jni', 'jni', duplicate = 0)
audio_env.VariantDir('$OBJDIR/samples', 'samples', duplicate = 0)
audio_env.VariantDir('$OBJDIR/test', 'test', duplicate = 0)

# Audio header install
audio_headers = audio_env.Install('$AUDIO_DISTDIR/inc/alljoyn/audio', audio_env.Glob('inc/alljoyn/audio/*.h'))
audio_headers += audio_env.Install('$AUDIO_DISTDIR/inc/alljoyn/audio/posix', audio_env.Glob('inc/alljoyn/audio/posix/*.h'))
audio_headers += audio_env.Install('$AUDIO_DISTDIR/inc/alljoyn/audio/android', audio_env.Glob('inc/alljoyn/audio/android/*.h'))

# Audio streaming libraries
libs = audio_env.SConscript('$OBJDIR/SConscript', exports = ['audio_env'])
audio_env.Install('$AUDIO_DISTDIR/lib', libs)

# Unit tests
audio_env.SConscript('unit_test/SConscript', variant_dir = '$OBJDIR/unittest', duplicate = 0, exports = ['audio_env'])

# Sample programs
if audio_env['BUILD_SERVICES_SAMPLES'] == 'on':
    audio_env.SConscript('$OBJDIR/samples/SConscript', exports = ['audio_env'])

# Release notes and misc. legals
if audio_env['OS_CONF'] == 'linux':
    audio_env.InstallAs('$AUDIO_DISTDIR/README.txt', 'docs/README.linux.txt')
    audio_env.Install('$AUDIO_DISTDIR', 'docs/ReleaseNotes.txt')
    audio_env.Install('$AUDIO_DISTDIR', 'README.md')
elif audio_env['OS_CONF'] == 'android':
    audio_env.InstallAs('$AUDIO_DISTDIR/README.txt', 'docs/README.android.txt')
    audio_env.Install('$AUDIO_DISTDIR', 'docs/ReleaseNotes.txt')
    audio_env.Install('$AUDIO_DISTDIR', 'README.md')

# Build docs
installDocs = audio_env.SConscript('docs/SConscript', exports = ['audio_env'])
audio_env.Depends(installDocs, audio_headers);
