rm -f live_push
g++ main.cpp  ffmpeg_transcode.cpp  -I /usr/local/include/ -Wdeprecated-declarations  -std=c++11 -lavcodec -lavformat -lswscale  -lswresample -lavutil -o live_push

