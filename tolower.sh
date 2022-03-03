#!/bin/bash

for F in `ls`; do
	mv "$F" "`echo "$F" | tr '[:upper:]' '[:lower:]'`"
done
