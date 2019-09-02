# GuitarTuner

## What it does

- get audio from mic
- resample the audio
- do fast fourier transformation every 0.1s, transform audio signal to frequence domain
- do [hps](https://cnx.org/contents/i5AAkZCP@2/Pitch-Detection-Algorithms), get fundamental frequency
- compare the obtained frequency whih standard frequency of note(C0 - B4)
- find the nearest standard note, get how flat or sharp obtained note from it

## Build and run

To build and run, need `gstreamer-1.0` and `PulseAudio`

```
gcc gt.c -o gt `pkg-config --cflags --libs gstreamer-1.0 ` -lm -g
```

## To do

- GUI
- support different tunings(Standard, Drop D, ...), rather than only auto.
