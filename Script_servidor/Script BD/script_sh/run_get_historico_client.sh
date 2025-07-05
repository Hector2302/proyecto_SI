#!/bin/bash
export GOOGLE_APPLICATION_CREDENTIALS="$HOME/proyecto/secure/firebase_decrypted/Service_Account_trigger_history.json"
python3 "$HOME/proyecto/scripts_python/trigger_save_data.py"
