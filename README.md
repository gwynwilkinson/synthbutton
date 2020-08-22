# synthbutton
August mini-project to build a software audio synth from scratch using C in Linux.

## Brief description
Real-time (to 10ms) audio synthesis of a sine wave using ALSA, libinput, signals and ncurses.

## ALSA Audio
Used to interface with sound card - https://www.alsa-project.org/wiki/Main_Page

Pretty easy to use, you just set up a PCM + parameters, set the number of channels, audio format, sample rate, period & buffer size, then it's ready.

PulseAudio seems to have superceded it in most distros but I couldn't actually find anything on how to use it + it seems more like a native multiplexer than an actual audio API + seems to be controversial as ALSA now handles multiplexing but everyone Just Uses Pulse Anyway. It would be worth picking this thread to see how it works better, and perhaps improve compatibility.

All audio problems currently are probably caused by errors in how timing and buffering is done.

Credit to http://equalarea.com/paul/alsa-audio.html and https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm_8c-example.html for giving me a starting point.

## Input.h
Used to read keyboard events

Set up a file handle for /dev/input/eventx and read input_events from it, and that's it. Maybe libinput is better somehow (https://wayland.freedesktop.org/libinput/doc/latest/) but this, too, seems controversial.

Figured out using https://thehackerdiary.wordpress.com/2017/04/21/exploring-devinput-1/ and https://github.com/freedesktop-unofficial-mirror/evtest/blob/master/evtest.c

## Signal.h
Used to handle audio synchronisation and interrupt handling in our single-threaded program.

We have two signal handlers - the first catches a realtime clock signal sent every 10ms (and prevents interruption during this), the second catches SIGINT so our input loop runs correctly.

As we are not threading, we use a mask to block signals while in the clock handler (failing to do this breaks the wave sampling function). Currently a bit of a blunt object as we block ALL signals - not sure what correct practice on this is.

Lots of tutorials on signal handling, man pages should be the starting point  - https://man7.org/linux/man-pages/man2/sigaction.2.html

## Time.h
Used for CLOCK_REALTIME, but it's not actually clear if this is worse than CLOCK_MONOTONIC. We need a clock signal to synchronise audio generation and this seems to be the closest thing possible.

Basically, we achieve realtime control by writing an audio buffer whenever a clock signal is received, then generating the buffer contents ready for the next clock signal. This doesn't work perfectly (I suspect snd_pcm_writei and sample_wave currently are both accessing the buffer and this is affecting things).

This way we can push our key and hear a sound without audible delay, giving the impression of real time responsiveness. (I read somewhere that 15ms is considered the upper latency limit for music software, which seems about right though I haven't tested this myself).

There is a realtime linux kernel which should make things more deterministic but I haven't tried it, it seemed like a rabbit hole and I didn't want to risk it for this small project.

Again man pages should be starting point eg. https://man7.org/linux/man-pages/man2/clock_gettime.2.html

## Ncurses.h
Used to hide terminal key echos and provide a simple user interface (to be added). Underused in this code but it's really nice, you just get a blank terminal screen that you get full control of (no scrolling, key echos etc).

It's pretty old school but there are some good tutorials on using it eg https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/ http://www.cs.ukzn.ac.za/~hughm/os/notes/ncurses.html

## Conclusions
I spent about three weeks on this in total, from learning how linux audio worked, experimenting with ALSA, working out how to use the keyboard to trigger sounds, realising that I would need to generate audio samples in realtime and researching how to do this with signals, putting it all together and then writing it up here. It would be nice to continue, but for now it's back to my PhD work.

The next steps as I envision them would be:

* Remove public variables
* Break main() up into functions
* Fix compatibility issues
* Fix probable buffer overrun issue causing snd_pcm_drain to hang
* Smooth playback (no pops or crackles in soundwave)
* Multiple key support
* Some different 'instruments' (square/triangle waves, strings/woodwinds/vocals, generalise standing waves somehow)
* Some sort of UX
* Investigate PulseAudio as alternative to ALSA

Despite the large amount of work left undone and the poor performance of the synth, I had a good time learning about all this and getting it sort-of-working. 

Hopefully this is useful and interesting to anyone reading, and feel free to take this and modify it however you want.
