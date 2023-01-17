#!/bin/bash

curl=$(curl -s -X POST -H "Content-Type: application/json" \
  -d '{ "source": { "dojo": "+hood/code %reset" }, "sink": { "app": "hood" } }' \
  http://127.0.0.1:12321)

if [[ $? -eq 0 ]]
then
  echo "OK"
else
  echo "Curl error: $?"
fi
