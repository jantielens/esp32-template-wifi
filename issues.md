### Title: mDNS stops responding after hours of uptime

**Description:**  
mDNS works shortly after boot but stops responding after some hours of uptime. IP access still works, and no WiFi reconnects have been observed.  

**Suspected Cause:**  
It is suspected that the issue may be due to a missing `MDNS.update()` call in the `loop`.  

**Recommendation:**  
- Add `MDNS.update()` when WiFi is connected.  
- Optionally call `MDNS.end()` before restarting mDNS.

**Relevant Code Links:**  
- [start_mdns() lines 483-566](https://github.com/jantielens/esp32-template-wifi/blob/main/src/app/app.ino#L483-L566)  
- [loop() lines 174-276](https://github.com/jantielens/esp32-template-wifi/blob/main/src/app/app.ino#L174-L276)  

---