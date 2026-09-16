from fastapi import FastAPI, HTTPException, UploadFile, File
from fastapi.responses import FileResponse
import ollama
import os
import re
import requests
import io
import wave
from pydub import AudioSegment
from faster_whisper import WhisperModel

app = FastAPI()

print("Loading Whisper model...")
whisper_model = WhisperModel("tiny.en", device="cpu", compute_type="int8")
print("Whisper model loaded!")

# --- 1. THE REFINED SYSTEM PROMPT ---
SORTING_HAT_PROMPT = (
    "You are the magical Sorting Hat from Hogwarts. "
    "Your primary goal is to have a natural, engaging conversation. "
    "You must draw your knowledge strictly from official Harry Potter canon, Pottermore, and the original books. "
    "Do not invent your own spells, lore, or magic systems. "
    "NEVER use parentheses, asterisks, or stage directions (like *laughs* or (pausing)). "
    "If you want to pause for dramatic effect, use ellipses (...) or em dashes (—). "
    "Maintain a wise, theatrical, and slightly grumpy tone. "
    "ONLY sort them into a house if they explicitly ask you to. "
    "Keep your answers conversational and brief—less than one or two short sentences."
    "You are allowed to answer in one word."
)

chat_history = []
MAX_HISTORY = 10 

@app.post("/chat")
async def chat_with_hat(audio_file: UploadFile = File(...)):
    global chat_history 
    
    try:
        incoming_audio_path = "incoming.wav"
        with open(incoming_audio_path, "wb") as buffer:
            buffer.write(await audio_file.read())
            
        print("Audio received! Transcribing...")

        segments, info = whisper_model.transcribe(incoming_audio_path, beam_size=5)
        user_text = "".join([segment.text for segment in segments])
        print(f"User said: {user_text}")

        if not user_text.strip():
            raise HTTPException(status_code=400, detail="No speech detected.")

        print("Thinking...")
        
        messages_payload = [{'role': 'system', 'content': SORTING_HAT_PROMPT}]
        messages_payload.extend(chat_history)
        messages_payload.append({'role': 'user', 'content': user_text})

        response = ollama.chat(model='llama3.2:1b', messages=messages_payload)
        reply_text = response['message']['content']
        
        # --- 2. THE REGEX BULLETPROOF FILTER ---
        # Strip out any rogue stage directions so they aren't spoken aloud
        reply_text = re.sub(r'\(.*?\)', '', reply_text)
        reply_text = re.sub(r'\*.*?\*', '', reply_text)
        
        print(f"Hat says: {reply_text}")

        chat_history.append({'role': 'user', 'content': user_text})
        chat_history.append({'role': 'assistant', 'content': reply_text})
        
        if len(chat_history) > MAX_HISTORY:
            chat_history = chat_history[-MAX_HISTORY:]

        # --- 3. THE ELEVENLABS UPGRADE ---
        print("Generating voice with ElevenLabs...")
        
        ELEVENLABS_API_KEY = "YOURAPIKEY"
        VOICE_ID = "YOUR VOICE IP From Eleven Labs"
        audio_filename = "response.wav"
        
        url = f"https://api.elevenlabs.io/v1/text-to-speech/{VOICE_ID}"
        headers = {
            "xi-api-key": ELEVENLABS_API_KEY,
            "Content-Type": "application/json"
        }
        data = {
            "text": reply_text,
            "model_id": "eleven_turbo_v2_5", # Turbo is optimized for fast conversational AI agents
            "voice_settings": {
                "stability": 0.5,
                "similarity_boost": 0.75
            }
        }
        
        tts_response = requests.post(url, json=data, headers=headers)
        
        if tts_response.status_code == 200:
            # Decode the ElevenLabs MP3 and convert it down to the 22050Hz audio the ESP32 needs
            audio_data = io.BytesIO(tts_response.content)
            sound = AudioSegment.from_file(audio_data, format="mp3")
            sound = sound.set_frame_rate(22050).set_channels(1).set_sample_width(2)
            
            # Safely write it using Python's native wave module to guarantee exactly a 44-byte header
            with wave.open(audio_filename, "wb") as wav_file:
                wav_file.setnchannels(1)
                wav_file.setsampwidth(2)
                wav_file.setframerate(22050)
                wav_file.writeframes(sound.raw_data)
                
            print("Done! Sending audio back.")
            return FileResponse(audio_filename, media_type="audio/wav")
        else:
            print(f"ElevenLabs API Error: {tts_response.text}")
            raise Exception("Failed to generate audio from ElevenLabs.")
            
    except Exception as e:
        print(f"Error: {e}")
        raise HTTPException(status_code=500, detail=str(e))

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
