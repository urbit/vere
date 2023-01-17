#!/bin/bash

raw=$(curl -s -X POST -H "Content-Type: application/json" \
  -d '{ "source": { "dojo": "+code" }, "sink": { "stdout": null } }' \
  http://127.0.0.1:12321)

# trim \n" from the end
trim="''${raw%\\n\"}"

# trim " from the start
code="''${trim#\"}"

echo "$code"
