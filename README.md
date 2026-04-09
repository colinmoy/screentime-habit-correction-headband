Step up instructions for ECE 445 project:

1. Open eim and go to dashboard
2. Open IDF terminal
3. cd into esp directory of project
4. run "idf.py fullclean"
5. run "idf.py set-target esp32c6"
6. run "idf.py build"
7. run "idf.py -p COM3 flash monitor" to view output from esp32 in terminal
8. verify all 3 tests pass

For app setup:
1. run "npx expo start --clear"
2. Scan generated QR code on phone
3. Connect to esp32c6 wifi named "Headband-AP" password:headband123
4. Go to Expo Go app
5. Input IP address
6. Press connect
