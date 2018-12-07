#!/bin/bash
IMAGE=eoslaomao/eos-dev:1.4.0
NAME=bankofstaked
FOLDER=bankofstaked

docker ps | grep $NAME-eos-dev
if [ $? -ne 0 ]; then
    echo "Run eos dev env "
    docker run --name $NAME-eos-dev -dit --rm -v  `(pwd)`:/$NAME $IMAGE
fi


docker exec $NAME-eos-dev eosio-cpp -abigen /$NAME/src/$NAME.cpp -o $NAME.wasm -I /contracts/eosio.token/include -I/contracts/eosio.system/include
docker exec $NAME-eos-dev cp /$NAME.abi /$NAME.wasm /$NAME/

if [ ! -d build ]; then
    mkdir build
    echo "Create build dir!!"
fi

mv $NAME.abi ./build
mv $NAME.wasm ./build
echo "Build SUCCESS!!!"


# For test and debug
docker exec nodeosd mkdir /$NAME
docker cp ../$FOLDER/$NAME.abi nodeosd:/$NAME/
docker cp ../$FOLDER/$NAME.wasm nodeosd:/$NAME/
docker cp ../$FOLDER/$NAME.wast nodeosd:/$NAME/
docker cp scripts nodeosd:/


##run unit test
#./unittest.sh
