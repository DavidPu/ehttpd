#!/bin/bash
#set -e

# ../out/bin/host_httpd &
# pid=$!
# echo $pid
# sleep 3

curl http://localhost:8888
curl http://localhost:8888/

# POST application/x-www-form-urlencoded
curl -d "param1=value1&param2=value2" -X POST http://localhost:8888/r/
for i in {1..3000}; do
    path=$(tr -dc '[:alnum:]/?&' < /dev/urandom |head -c $i)
    curl http://localhost:8888/${path}
done
