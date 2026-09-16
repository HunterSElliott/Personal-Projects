# Sorting Hat Animatronic

The Harry Potter Sorting Hat has always been a point of curiosity such as, "which house would I be in?", or "What would the hat say about me?"

I took this inspiration and created the hat in high school for a Drama competition.
This hat was made out of an old coat of mine. It was a leather coat that I Loved. I didn't want to get rid of it when it started ripping so I instead transformed it into the Sorting Hat.

# The initial

Initially, this project was meant to be remote controlled so that I could move the hat remotely while a user was wearing it and activate pre-made audio files to sort the user into a given house. I did not know where to start with working with servos and ran out of time before the competition so instead I turned it into a puppet with a speaker inside of it for the audios.

# Update to now

I have enrolled in university in Electrical Engineering and am starting to gain skills that could lead to more of what I had initially wanted. But better.
I have a python script on a Raspberry Pi 5 (8GB) that tells an Ollama model how to think. There is a Whisper model that transcribes an audio file and a connection to eleven labs to generate a voice. The Raspberry Pi talks wirelessly to an ESP32-S3 that controls the body.

# Hardware

* ESP32-S3
* Raspberyy Pi 5 (8GB)
* Motor Controller PCA9685
* Microphone - INMP441
* Speaker - 3 Watt 8 Ohm Mini Speaker
* Amplifier - MAX98357A
* Battery - SCX24 Battery 7.4V
