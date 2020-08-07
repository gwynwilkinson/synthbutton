#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

main (int argc, char *argv[]){

    int i, err;
    short buf[128];
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params;

    /* Sets playback handle to point to new Pulse Code Modulator (PCM)
            using args, PCM stream direction flag (playback), and no */ 
    if ((err = snd_pcm_open (&playback_handle, argv[1], SND_PCM_STREAM_PLAYBACK, 0)) < 0){
        fprintf (stderr, "Cannot open audio device %s (%s)\n",
                 argv[1],
                 snd_strerror (err));
        exit(1);
    }

}
