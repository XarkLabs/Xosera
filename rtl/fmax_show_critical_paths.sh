#! /bin/bash
echo === Source signals: ===
ggrep -hoP '(?<=Source\s)\w+' upduino/logs/fmax/*_fMAX*.log | sort | uniq -c | sort -rn
echo === Net signals:    ===
ggrep -hoP '(?<=Net\s)\w+' upduino/logs/fmax/*_fMAX*.log | sort | uniq -c | sort -rn
echo === Sink signals:   ===
ggrep -hoP '(?<=Sink\s)\w+' upduino/logs/fmax/*_fMAX*.log | sort | uniq -c | sort -rn
