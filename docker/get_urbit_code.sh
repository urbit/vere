#!/bin/bash

code=$(curl -s -X POST -H "Content-Type: application/json" \
  -d '{ "source": { "dojo": "+code" }, "sink": { "stdout": null } }' \
  http://127.0.0.1:12321)

# Trim newlines and double quotes.
echo "$code" | sed 's/\\n//' | tr -d '"'
