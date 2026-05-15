import midas.client
import time

if __name__ == "__main__":
    client = midas.client.MidasClient("event-reader")
    
    buffer_handle = client.open_event_buffer("SYSTEM")
    
    request_id = client.register_event_request(buffer_handle, event_id = 1)
    
    while True:
        event = client.receive_event(buffer_handle, async_flag=True)
        
        if event is not None:
            bank_names = ", ".join(b.name for b in event.banks.values())
            # print("Received event with timestamp %s containing banks %s" % (event.header.timestamp, bank_names))
            
        client.communicate(10)
