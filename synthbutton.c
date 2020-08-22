//  synthbutton.c
//  🄯Gwyn Wilkinson
//  Summer 2020 mini-project

//  80-char max width not observed, but feel free to rewrite this for your 1970s punchcard mainframe, nerd

//  EXPERIMENT & EDUCATION ONLY - VERSION 0.1
//  Works on Linux Mint with Xonar U7 and Bose QC35 IIs, but not onboard speakers somehow

//  TODO:   remove public variables
//          break main() into functions
//          fix compatibility issues
//          fix probable buffer overrun issue causing snd_pcm_drain to hang
//          smooth playback (no pops or crackles in soundwave)
//          multiple key support
//          some sort of UX
//          investigate PulseAudio as alternative to ALSA

//  Compile: $ gcc synthbutton.c -lm -lasound -lrt -lncurses



#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h> // open()
#include <unistd.h> // read()
#include <alsa/asoundlib.h> // sound card interactions
#include <math.h>   //  sine wave function
#include <time.h>   //  clock functions
#include <errno.h>  //  defines errno
#include <ncurses.h>    //  text-based UI
#include <signal.h> //  signal operations
#include <linux/input.h> // input_event stuff

// realtime clock - TODO: seems unsafe to do this rather than just use CLOCK_REALTIME directly
#define CLOCKID CLOCK_REALTIME
#define SIG SIGRTMIN    //realtime signal minimum

int err;    //  used for alsa returns (TODO)

unsigned int sample_rate = 44100;   //  set here for convenience
int dir = 0;    //  Used in ALSA calls, I assume means direction but docs say little about it

unsigned int frames_in_sample = 441; //  set here for convenience (44100Hz/10ms)

static volatile sig_atomic_t keep_running = 1;  //  while loop flag
timer_t timerid;
struct sigevent sev;
struct itimerspec its;
struct sigaction sa;    //  timer signal handler

sigset_t allmask;
sigset_t orig_mask;
struct sigaction sint;  //  interrupt signal handler


snd_pcm_t *playback_handle; //  sound card
snd_pcm_hw_params_t *hw_params; //  sound card parameters
snd_pcm_uframes_t frames_in_period = 441;   //  same as frames_in_sample? set here for convenience.
snd_pcm_uframes_t frames_in_pcm_buffer = 441*4; //  buffer holds 4 periods. set here for convenience.


char buffer[441*4]; //  set here for convenience - TODO: calloc this instead

//  wave struct:
//  boolean flag array for whether key is on or off
//  boolean flag array for whether we play note or not
//  uint array for timestamps
//  uint array for frequency
//  uint array for lowest common multiple of frequency and sample rate (so we can capture the whole wave)

struct keystruct {
    bool keydown;
    bool isplaying;
    unsigned int timestamp;
    unsigned int frequency;
    unsigned int lcm;
};

// defined here for convenience
struct keystruct A = {false,false,0,220,485100};

//  timer signal handler
//  catch timer signal and send buffer contents to pcm
//  call wave sampler and send pointer to our flag/timestamp struct

void sample_wave(){
    double t,x,y;
    int amp=10000,sample=0,temp=0;

    if (A.keydown==1){
        // Calculate sine wave for each slice of this 10ms sample
        for(int i=0;i<frames_in_sample;i++){
            temp = i + A.timestamp;
            temp = temp % A.lcm;
            t = (double)temp;
            x = t/(double)sample_rate;
            y = sin(2.0*3.14159*(double)A.frequency*x);
            sample = amp * y;

            //  2-channel 16-bit little-endian
            buffer[0 + 4*i] = sample & 0xff;
            buffer[1 + 4*i] = (sample & 0xff00) >> 8;
            buffer[2 + 4*i] = sample & 0xff;
            buffer[3 + 4*i] = (sample & 0xff00) >> 8;
        }
        A.timestamp += frames_in_sample;    //  Move our wave timestamp along
    }else{
        for(int i=0;i<frames_in_sample;i++){
            // TODO - change this to continue wave until we cross y=0, then fill rest with 0
            sample=0;
            buffer[0 + 4*i] = sample & 0xff;
            buffer[1 + 4*i] = (sample & 0xff00) >> 8;
            buffer[2 + 4*i] = sample & 0xff;
            buffer[3 + 4*i] = (sample & 0xff00) >> 8;
        }

    }

}

static void sig_timer_handler(int sig, siginfo_t *si, void *uc){
   /* Note: calling printf() from a signal handler is not safe
      (and should not be done in production programs), since
      printf() is not async-signal-safe; see signal-safety(7). */

      //    We're not using multithreading so we have to block interrupts while we're here
      sigprocmask(SIG_BLOCK, &allmask, &orig_mask);

      //    TODO - work out how to do this without public declared handles
      //    send current buffer to sound card
      err = snd_pcm_writei (playback_handle, buffer, frames_in_period);

      if (err < 0){
          //    If it doesn't work, call snd_pcm_prepare() - TODO: This feels hackish & probably wrong
          snd_pcm_prepare(playback_handle);
      }

      //    create next 10ms sample - TODO send buffer as argument
      sample_wave();

      //    ready to handle interrupts again
      sigprocmask(SIG_SETMASK, &orig_mask, NULL);
}

//  interrupt signal handler
//  TODO - compiler doesn't like this being called as sa_sigaction for some reason
static void sig_int_handler(int _)
{
    (void)_;    //  TODO - find out why this is in example code.
    keep_running = 0;
    endwin();   //  end ncurses and return to terminal - TODO: move to after while loop
    printf("exit\n");
}

//  main
//  set up pcm handle and params
//  set up timer and sigint
//  sit in while_running loop until sigint is caught
//  close pcm and return 0
int main(){

    char str[80];   //  ncurses message output
    int row,col;    //  used by ncurses

    //  TODO - get this working (better to zero-initialise our buffer)
    //buffer = (char*)calloc(441*4,sizeof(char));

    //  ALSA sound card setup - TODO: Put error guards around this, consider porting to PulseAudio for comparison's sake
    snd_pcm_open (&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    snd_pcm_hw_params_malloc (&hw_params);
    snd_pcm_hw_params_any (playback_handle, hw_params);
    snd_pcm_hw_params_set_access (playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format (playback_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels (playback_handle, hw_params, 2);
    snd_pcm_hw_params_set_rate (playback_handle, hw_params, sample_rate, dir);
    snd_pcm_hw_params_set_period_size (playback_handle, hw_params, frames_in_period, dir);
    snd_pcm_hw_params_set_buffer_size (playback_handle, hw_params, frames_in_pcm_buffer);
    snd_pcm_hw_params (playback_handle, hw_params);
    snd_pcm_hw_params_free (hw_params);
    snd_pcm_prepare(playback_handle);

    //  Set all flags in &allmask
    sigfillset(&allmask);

    //  Setting up timer signal handler
    sa.sa_flags = SA_SIGINFO;    // this flag means we are using sa_sigaction (not sa_handler)
    sa.sa_sigaction = sig_timer_handler;   // confusingly we then call our sigaction 'handler', whatever
    sigemptyset(&sa.sa_mask);    //initialize signal set with no flags set

    /* "The sigaction() system call is used to change the action taken by a
        process on receipt of a specific signal." */
    if (sigaction(SIG, &sa, NULL) == -1){
        printf("sigaction");
        return 0;
    }

    /* Create the timer */
    // sev is our sigevent.
    sev.sigev_notify = SIGEV_SIGNAL; //Notify the process by sending the signal specified in sev.sigev_signo.
    sev.sigev_signo = SIG;   // SIG = SIGRTMIN
    sev.sigev_value.sival_ptr = &timerid;    //value pointer set to point to timer
    if (timer_create(CLOCKID, &sev, &timerid) == -1){   //  CLOCKID is CLOCK_REALTIME
        printf("timer_create");
        return 0;
    }

    //  Wait 1 second, then fire every 10ms.
    //  This is really fiddly but seems to be the correct procedure.
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 1000000000 % 1000000000;
    its.it_interval.tv_sec = 10000000 / 1000000000;
    its.it_interval.tv_nsec = 10000000 % 1000000000;

    if (timer_settime(timerid, 0, &its, NULL) == -1){
        printf("timer_settime");
        return 0;
    }

    // create interrupt signal handler
    sint.sa_flags = SA_SIGINFO; // This should let us set sa_sigaction
    sint.sa_handler = sig_int_handler;  //  for some reason though sa_sigaction gives a compiler error, so use sa_handler
    sigemptyset(&sint.sa_mask); //  Create new empty signal set
    sigaddset (&sint.sa_mask, SIGINT);  //  Add SIGINT to new empty set

    sigaction(SIGINT,&sint,NULL);   //  Start catching sigint


    int fd, rd, i;
    struct input_event ev[64];  //  libinput struct

    //  event4 is the keyboard. We need super user access or this fails
    if ((fd = open("/dev/input/event4", O_RDONLY)) < 0) {
			perror("Keyboard access");
			return 1;
	}

    initscr();  //  start the ncurses mode
	noecho();   //  disable terminal output while in ncurses mode
    getmaxyx(stdscr,row,col);   //  calculate ncurses screen size

    //  do this until sigint (i.e. Ctrl+C pressed)
    while (keep_running){
        //  read key events
        rd = read(fd, ev, sizeof(struct input_event) * 64);

        //  TODO - read further into EINTR handling etc as it seems incorrect, but works for now
		if (rd < (int) sizeof(struct input_event)) {
            if(errno==EINTR){
                //nothing?
            }else{
                perror("\nevtest: error reading");
    			return 1;
            }
        //  read() doesn't fail somehow
		}else{
            //  Each keypress generates 3 events so get the right one
            for (i = 0; i < rd / sizeof(struct input_event); i++){

                //  type 1 means that code contains the actual key ID
    			if(ev[i].type==1 & ev[i].code == 30){

                    //  0 is keyup, 1 is keydown, 2 is keyhold (this is so easy, thank you team libinput)
                    //  TODO - nicer semantics here (i.e. A.keydown=ev.value), multiple keys (A to G) so we can play chords
                    if(ev[i].value==0){
                        A.keydown=false;
                    }else if(ev[i].value==1){
                        A.keydown=true;
                    }
                }
            }
        }


    }

    //  After we exit loop, play remaining samples and close sound card
    //  TODO - this currently breaks if we run for too long, pretty sure we have a buffer overrun issue.
    snd_pcm_drain (playback_handle);
    snd_pcm_close (playback_handle);

    return 0;
}
