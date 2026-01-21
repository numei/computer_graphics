

if [ ! -d "build" ];then
  mkdir build
  else
  echo "build/ already exists"
fi
cd build 
cmake .. 
make 
./HelloGL
