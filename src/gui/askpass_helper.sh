#!/bin/bash
# Remus askpass helper for sudo authentication
# This script displays a password dialog using osascript

osascript -e 'display dialog "Remus needs administrator privileges to access USB devices:" default answer "" with title "Remus Authentication" with icon caution with hidden answer' -e 'text returned of result' 2>/dev/null
