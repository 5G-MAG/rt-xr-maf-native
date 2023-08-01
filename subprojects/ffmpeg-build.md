git clone https://github.com/BtbN/FFmpeg-Builds.git 
cd FFmpeg-Builds
./makeimage.sh win64 lgpl-shared 5.1
./build.sh target win64 lgpl-shared 5.1
ls -l artifacts/
