import threading
import requests
import time

URL = "http://localhost:8080/api/download"
# Use a very short video or a valid URL for testing
TEST_URL = "https://www.youtube.com/watch?v=jNQXAC9IVRw" 

def send_request(i):
    print(f"[Thread {i}] Sending request...")
    try:
        response = requests.post(URL, json={"url": TEST_URL})
        print(f"[Thread {i}] Status: {response.status_code}, Body: {response.content.decode()}")
    except Exception as e:
        print(f"[Thread {i}] Error: {e}")

threads = []
print("Starting Burst Stress Test...")
for i in range(5): # Start with 5 parallel requests
    t = threading.Thread(target=send_request, args=(i,))
    threads.append(t)
    t.start()
    time.sleep(0.1) # slight delay to stagger info json writing

for t in threads:
    t.join()

print("\nChecking Server Status...")
status_res = requests.get("http://localhost:8080/api/status")
print(f"Server Status: {status_res.content.decode()}")
