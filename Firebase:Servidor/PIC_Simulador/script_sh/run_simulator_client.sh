#!/bin/bash
export GOOGLE_APPLICATION_CREDENTIALS="$HOME/proyecto/secure/firebase_decrypted/Service_Account_PIC.json"
python3 "$HOME/proyecto/scripts_python/simulador_anti_incendios.py"
