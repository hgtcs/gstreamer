# gstreamer
gstreamer: v4l2src/filesrc x264enc appsink

### Makefile:
This Makefile is generic for all *.c files below.  
For each *.c file, just a simple modification is required.  
v4l2-enc-appsink.c     
file-enc-appsink.c

### v4l2-enc-appsink.c:
The gstreamer pipeline is v4l2src--srccapsfilter--timeoverlay--x264enc--enc_queue--appsink.      
It will save 20s of the h264 video.

### file-enc-appsink.c
The gstreamer pipeline is filesrc--videoparse--timeoverlay--x264enc--enc_queue--appsink.  
