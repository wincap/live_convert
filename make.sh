rm -f live_push
g++ main.cpp  ffmpeg_transcode.cpp -Wdeprecated-declarations  -I /usr/local/include/ -std=c++11 -lavcodec -lavformat -lswscale  -lswresample -lavutil -o live_push

