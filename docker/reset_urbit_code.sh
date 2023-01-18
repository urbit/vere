#!/bin/bash

# Sending `+hood/code %reset` to `dojo` resets the `+code`, but it requires
# interactive confirmation to do so. Obviously, we don't want to have to provide
# that confirmation, so we force reset the `+code` using
# `+hood/pass [%j %step ~]`, credit for which goes to ~wicdev-wisryt.
resp=$(curl -s -X POST -H "Content-Type: application/json"                            \
  -d '{ "source": { "dojo": "+hood/pass [%j %step ~]" }, "sink": { "app": "hood" } }' \
  http://127.0.0.1:12321)

if [[ $? -eq 0 ]]
then
  echo "OK"
else
  echo "Curl error: $?"
fi
